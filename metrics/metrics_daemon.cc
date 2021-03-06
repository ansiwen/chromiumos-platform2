// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metrics/metrics_daemon.h"

#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/hash.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/sys_info.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus.h>
#include <dbus/message.h>
#include "uploader/upload_service.h"

using base::FilePath;
using base::StringPrintf;
using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using chromeos_metrics::PersistentInteger;
using std::map;
using std::string;
using std::vector;

namespace {

#define SAFE_MESSAGE(e) (e.message ? e.message : "unknown error")

const char kCrashReporterInterface[] = "org.chromium.CrashReporter";
const char kCrashReporterUserCrashSignal[] = "UserCrash";
const char kCrashReporterMatchRule[] =
    "type='signal',interface='%s',path='/',member='%s'";

// Build type of an official build.
// See chromite/scripts/cros_set_lsb_release.py.
const char kOfficialBuild[] = "Official Build";

const int kSecondsPerMinute = 60;
const int kMinutesPerHour = 60;
const int kHoursPerDay = 24;
const int kMinutesPerDay = kHoursPerDay * kMinutesPerHour;
const int kSecondsPerDay = kSecondsPerMinute * kMinutesPerDay;
const int kDaysPerWeek = 7;
const int kSecondsPerWeek = kSecondsPerDay * kDaysPerWeek;

// Interval between calls to UpdateStats().
const uint32_t kUpdateStatsIntervalMs = 300000;

// Maximum amount of system memory that will be reported without overflow.
const int kMaximumMemorySizeInKB = 32 * 1000 * 1000;

const char kKernelCrashDetectedFile[] = "/run/kernel-crash-detected";
const char kUncleanShutdownDetectedFile[] =
    "/run/unclean-shutdown-detected";

}  // namespace

// disk stats metrics

// The {Read,Write}Sectors numbers are in sectors/second.
// A sector is usually 512 bytes.

const char MetricsDaemon::kMetricReadSectorsLongName[] =
    "Platform.ReadSectorsLong";
const char MetricsDaemon::kMetricWriteSectorsLongName[] =
    "Platform.WriteSectorsLong";
const char MetricsDaemon::kMetricReadSectorsShortName[] =
    "Platform.ReadSectorsShort";
const char MetricsDaemon::kMetricWriteSectorsShortName[] =
    "Platform.WriteSectorsShort";

const int MetricsDaemon::kMetricStatsShortInterval = 1;  // seconds
const int MetricsDaemon::kMetricStatsLongInterval = 30;  // seconds

const int MetricsDaemon::kMetricMeminfoInterval = 30;        // seconds

// Assume a max rate of 250Mb/s for reads (worse for writes) and 512 byte
// sectors.
const int MetricsDaemon::kMetricSectorsIOMax = 500000;  // sectors/second
const int MetricsDaemon::kMetricSectorsBuckets = 50;    // buckets
// Page size is 4k, sector size is 0.5k.  We're not interested in page fault
// rates that the disk cannot sustain.
const int MetricsDaemon::kMetricPageFaultsMax = kMetricSectorsIOMax / 8;
const int MetricsDaemon::kMetricPageFaultsBuckets = 50;

// Major page faults, i.e. the ones that require data to be read from disk.

const char MetricsDaemon::kMetricPageFaultsLongName[] =
    "Platform.PageFaultsLong";
const char MetricsDaemon::kMetricPageFaultsShortName[] =
    "Platform.PageFaultsShort";

// Swap in and Swap out

const char MetricsDaemon::kMetricSwapInLongName[] =
    "Platform.SwapInLong";
const char MetricsDaemon::kMetricSwapInShortName[] =
    "Platform.SwapInShort";

const char MetricsDaemon::kMetricSwapOutLongName[] =
    "Platform.SwapOutLong";
const char MetricsDaemon::kMetricSwapOutShortName[] =
    "Platform.SwapOutShort";

const char MetricsDaemon::kMetricsProcStatFileName[] = "/proc/stat";
const int MetricsDaemon::kMetricsProcStatFirstLineItemsCount = 11;

// Thermal CPU throttling.

const char MetricsDaemon::kMetricScaledCpuFrequencyName[] =
    "Platform.CpuFrequencyThermalScaling";

// Zram sysfs entries.

const char MetricsDaemon::kComprDataSizeName[] = "compr_data_size";
const char MetricsDaemon::kOrigDataSizeName[] = "orig_data_size";
const char MetricsDaemon::kZeroPagesName[] = "zero_pages";

// crouton metrics

const char MetricsDaemon::kMetricCroutonStarted[] = "Platform.Crouton.Started";

// Memory use stats collection intervals.  We collect some memory use interval
// at these intervals after boot, and we stop collecting after the last one,
// with the assumption that in most cases the memory use won't change much
// after that.
static const int kMemuseIntervals[] = {
  1 * kSecondsPerMinute,    // 1 minute mark
  4 * kSecondsPerMinute,    // 5 minute mark
  25 * kSecondsPerMinute,   // 0.5 hour mark
  120 * kSecondsPerMinute,  // 2.5 hour mark
  600 * kSecondsPerMinute,  // 12.5 hour mark
};

MetricsDaemon::MetricsDaemon()
    : memuse_final_time_(0),
      memuse_interval_index_(0),
      read_sectors_(0),
      write_sectors_(0),
      vmstats_(),
      stats_state_(kStatsShort),
      stats_initial_time_(0),
      ticks_per_second_(0),
      latest_cpu_use_ticks_(0) {}

MetricsDaemon::~MetricsDaemon() {
}

double MetricsDaemon::GetActiveTime() {
  struct timespec ts;
  int r = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (r < 0) {
    PLOG(WARNING) << "clock_gettime(CLOCK_MONOTONIC) failed";
    return 0;
  } else {
    return ts.tv_sec + static_cast<double>(ts.tv_nsec) / (1000 * 1000 * 1000);
  }
}

int MetricsDaemon::Run() {
  if (CheckSystemCrash(kKernelCrashDetectedFile)) {
    ProcessKernelCrash();
  }

  if (CheckSystemCrash(kUncleanShutdownDetectedFile)) {
    ProcessUncleanShutdown();
  }

  // On OS version change, clear version stats (which are reported daily).
  int32_t version = GetOsVersionHash();
  if (version_cycle_->Get() != version) {
    version_cycle_->Set(version);
    kernel_crashes_version_count_->Set(0);
    version_cumulative_active_use_->Set(0);
    version_cumulative_cpu_use_->Set(0);
  }

  return brillo::DBusDaemon::Run();
}

void MetricsDaemon::RunUploaderTest() {
  upload_service_.reset(new UploadService(new SystemProfileCache(true,
                                                                 config_root_),
                                          metrics_lib_,
                                          server_));
  upload_service_->Init(upload_interval_, metrics_file_);
  upload_service_->UploadEvent();
}

uint32_t MetricsDaemon::GetOsVersionHash() {
  static uint32_t cached_version_hash = 0;
  static bool version_hash_is_cached = false;
  if (version_hash_is_cached)
    return cached_version_hash;
  version_hash_is_cached = true;
  std::string version;
  if (base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_VERSION", &version)) {
    cached_version_hash = base::Hash(version);
  } else if (testing_) {
    cached_version_hash = 42;  // return any plausible value for the hash
  } else {
    LOG(FATAL) << "could not find CHROMEOS_RELEASE_VERSION";
  }
  return cached_version_hash;
}

bool MetricsDaemon::IsOnOfficialBuild() const {
  std::string build_type;
  return (base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_BUILD_TYPE",
                                            &build_type) &&
          build_type == kOfficialBuild);
}

void MetricsDaemon::Init(bool testing,
                         bool uploader_active,
                         MetricsLibraryInterface* metrics_lib,
                         const string& diskstats_path,
                         const string& vmstats_path,
                         const string& scaling_max_freq_path,
                         const string& cpuinfo_max_freq_path,
                         const base::TimeDelta& upload_interval,
                         const string& server,
                         const string& metrics_file,
                         const string& config_root) {
  testing_ = testing;
  uploader_active_ = uploader_active;
  config_root_ = config_root;
  DCHECK(metrics_lib != nullptr);
  metrics_lib_ = metrics_lib;

  upload_interval_ = upload_interval;
  server_ = server;
  metrics_file_ = metrics_file;

  // Get ticks per second (HZ) on this system.
  // Sysconf cannot fail, so no sanity checks are needed.
  ticks_per_second_ = sysconf(_SC_CLK_TCK);

  daily_active_use_.reset(
      new PersistentInteger("Platform.DailyUseTime"));
  version_cumulative_active_use_.reset(
      new PersistentInteger("Platform.CumulativeUseTime"));
  version_cumulative_cpu_use_.reset(
      new PersistentInteger("Platform.CumulativeCpuTime"));

  kernel_crash_interval_.reset(
      new PersistentInteger("Platform.KernelCrashInterval"));
  unclean_shutdown_interval_.reset(
      new PersistentInteger("Platform.UncleanShutdownInterval"));
  user_crash_interval_.reset(
      new PersistentInteger("Platform.UserCrashInterval"));

  any_crashes_daily_count_.reset(
      new PersistentInteger("Platform.AnyCrashesDaily"));
  any_crashes_weekly_count_.reset(
      new PersistentInteger("Platform.AnyCrashesWeekly"));
  user_crashes_daily_count_.reset(
      new PersistentInteger("Platform.UserCrashesDaily"));
  user_crashes_weekly_count_.reset(
      new PersistentInteger("Platform.UserCrashesWeekly"));
  kernel_crashes_daily_count_.reset(
      new PersistentInteger("Platform.KernelCrashesDaily"));
  kernel_crashes_weekly_count_.reset(
      new PersistentInteger("Platform.KernelCrashesWeekly"));
  kernel_crashes_version_count_.reset(
      new PersistentInteger("Platform.KernelCrashesSinceUpdate"));
  unclean_shutdowns_daily_count_.reset(
      new PersistentInteger("Platform.UncleanShutdownsDaily"));
  unclean_shutdowns_weekly_count_.reset(
      new PersistentInteger("Platform.UncleanShutdownsWeekly"));

  daily_cycle_.reset(new PersistentInteger("daily.cycle"));
  weekly_cycle_.reset(new PersistentInteger("weekly.cycle"));
  version_cycle_.reset(new PersistentInteger("version.cycle"));

  diskstats_path_ = diskstats_path;
  vmstats_path_ = vmstats_path;
  scaling_max_freq_path_ = scaling_max_freq_path;
  cpuinfo_max_freq_path_ = cpuinfo_max_freq_path;

  // If testing, initialize Stats Reporter without connecting DBus
  if (testing_)
    StatsReporterInit();
}

int MetricsDaemon::OnInit() {
  int return_code = brillo::DBusDaemon::OnInit();
  if (return_code != EX_OK)
    return return_code;

  StatsReporterInit();

  // Start collecting meminfo stats.
  ScheduleMeminfoCallback(kMetricMeminfoInterval);
  memuse_final_time_ = GetActiveTime() + kMemuseIntervals[0];
  ScheduleMemuseCallback(kMemuseIntervals[0]);

  if (testing_)
    return EX_OK;

  bus_->AssertOnDBusThread();
  CHECK(bus_->SetUpAsyncOperations());

  if (bus_->is_connected()) {
    const std::string match_rule =
        base::StringPrintf(kCrashReporterMatchRule,
                           kCrashReporterInterface,
                           kCrashReporterUserCrashSignal);

    bus_->AddFilterFunction(&MetricsDaemon::MessageFilter, this);

    DBusError error;
    dbus_error_init(&error);
    bus_->AddMatch(match_rule, &error);

    if (dbus_error_is_set(&error)) {
      LOG(ERROR) << "Failed to add match rule \"" << match_rule << "\". Got "
          << error.name << ": " << error.message;
      return EX_SOFTWARE;
    }
  } else {
    LOG(ERROR) << "DBus isn't connected.";
    return EX_UNAVAILABLE;
  }

  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&MetricsDaemon::HandleUpdateStatsTimeout,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kUpdateStatsIntervalMs));

  // Emit a "0" value on start, to provide a baseline for this metric.
  SendLinearSample(kMetricCroutonStarted, 0, 2, 3);
  SendCroutonStats();

  if (uploader_active_) {
    if (IsOnOfficialBuild()) {
      LOG(INFO) << "uploader enabled";
      upload_service_.reset(
          new UploadService(new SystemProfileCache(), metrics_lib_, server_));
      upload_service_->Init(upload_interval_, metrics_file_);
    } else {
      LOG(INFO) << "uploader disabled on non-official build";
    }
  }

  return EX_OK;
}

void MetricsDaemon::OnShutdown(int* return_code) {
  if (!testing_ && bus_->is_connected()) {
    const std::string match_rule =
        base::StringPrintf(kCrashReporterMatchRule,
                           kCrashReporterInterface,
                           kCrashReporterUserCrashSignal);

    bus_->RemoveFilterFunction(&MetricsDaemon::MessageFilter, this);

    DBusError error;
    dbus_error_init(&error);
    bus_->RemoveMatch(match_rule, &error);

    if (dbus_error_is_set(&error)) {
      LOG(ERROR) << "Failed to remove match rule \"" << match_rule << "\". Got "
          << error.name << ": " << error.message;
    }
  }
  brillo::DBusDaemon::OnShutdown(return_code);
}

// static
DBusHandlerResult MetricsDaemon::MessageFilter(DBusConnection* connection,
                                               DBusMessage* message,
                                               void* user_data) {
  int message_type = dbus_message_get_type(message);
  if (message_type != DBUS_MESSAGE_TYPE_SIGNAL) {
    DLOG(WARNING) << "unexpected message type " << message_type;
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  // Signal messages always have interfaces.
  const std::string interface(dbus_message_get_interface(message));
  const std::string member(dbus_message_get_member(message));
  DLOG(INFO) << "Got " << interface << "." << member << " D-Bus signal";

  MetricsDaemon* daemon = static_cast<MetricsDaemon*>(user_data);

  DBusMessageIter iter;
  dbus_message_iter_init(message, &iter);
  if (interface == kCrashReporterInterface) {
    CHECK_EQ(member, kCrashReporterUserCrashSignal);
    daemon->ProcessUserCrash();
  } else {
    // Ignore messages from the bus itself.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

// One might argue that parts of this should go into
// chromium/src/base/sys_info_chromeos.c instead, but put it here for now.

TimeDelta MetricsDaemon::GetIncrementalCpuUse() {
  FilePath proc_stat_path = FilePath(kMetricsProcStatFileName);
  std::string proc_stat_string;
  if (!base::ReadFileToString(proc_stat_path, &proc_stat_string)) {
    LOG(WARNING) << "cannot open " << kMetricsProcStatFileName;
    return TimeDelta();
  }

  std::vector<std::string> proc_stat_lines =
      base::SplitString(proc_stat_string, "\n", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  if (proc_stat_lines.empty()) {
    LOG(WARNING) << "cannot parse " << kMetricsProcStatFileName
                 << ": " << proc_stat_string;
    return TimeDelta();
  }
  std::vector<std::string> proc_stat_totals =
      base::SplitString(proc_stat_lines[0], base::kWhitespaceASCII,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  uint64_t user_ticks, user_nice_ticks, system_ticks;
  if (proc_stat_totals.size() != kMetricsProcStatFirstLineItemsCount ||
      proc_stat_totals[0] != "cpu" ||
      !base::StringToUint64(proc_stat_totals[1], &user_ticks) ||
      !base::StringToUint64(proc_stat_totals[2], &user_nice_ticks) ||
      !base::StringToUint64(proc_stat_totals[3], &system_ticks)) {
    LOG(WARNING) << "cannot parse first line: " << proc_stat_lines[0];
    return TimeDelta(base::TimeDelta::FromSeconds(0));
  }

  uint64_t total_cpu_use_ticks = user_ticks + user_nice_ticks + system_ticks;

  // Sanity check.
  if (total_cpu_use_ticks < latest_cpu_use_ticks_) {
    LOG(WARNING) << "CPU time decreasing from " << latest_cpu_use_ticks_
                 << " to " << total_cpu_use_ticks;
    return TimeDelta();
  }

  uint64_t diff = total_cpu_use_ticks - latest_cpu_use_ticks_;
  latest_cpu_use_ticks_ = total_cpu_use_ticks;
  // Use microseconds to avoid significant truncations.
  return base::TimeDelta::FromMicroseconds(
      diff * 1000 * 1000 / ticks_per_second_);
}

void MetricsDaemon::ProcessUserCrash() {
  // Counts the active time up to now.
  UpdateStats(TimeTicks::Now(), Time::Now());

  // Reports the active use time since the last crash and resets it.
  SendAndResetCrashIntervalSample(user_crash_interval_);

  any_crashes_daily_count_->Add(1);
  any_crashes_weekly_count_->Add(1);
  user_crashes_daily_count_->Add(1);
  user_crashes_weekly_count_->Add(1);
}

void MetricsDaemon::ProcessKernelCrash() {
  // Counts the active time up to now.
  UpdateStats(TimeTicks::Now(), Time::Now());

  // Reports the active use time since the last crash and resets it.
  SendAndResetCrashIntervalSample(kernel_crash_interval_);

  any_crashes_daily_count_->Add(1);
  any_crashes_weekly_count_->Add(1);
  kernel_crashes_daily_count_->Add(1);
  kernel_crashes_weekly_count_->Add(1);

  kernel_crashes_version_count_->Add(1);
}

void MetricsDaemon::ProcessUncleanShutdown() {
  // Counts the active time up to now.
  UpdateStats(TimeTicks::Now(), Time::Now());

  // Reports the active use time since the last crash and resets it.
  SendAndResetCrashIntervalSample(unclean_shutdown_interval_);

  unclean_shutdowns_daily_count_->Add(1);
  unclean_shutdowns_weekly_count_->Add(1);
  any_crashes_daily_count_->Add(1);
  any_crashes_weekly_count_->Add(1);
}

bool MetricsDaemon::CheckSystemCrash(const string& crash_file) {
  FilePath crash_detected(crash_file);
  if (!base::PathExists(crash_detected))
    return false;

  // Deletes the crash-detected file so that the daemon doesn't report
  // another kernel crash in case it's restarted.
  base::DeleteFile(crash_detected, false);  // not recursive
  return true;
}

void MetricsDaemon::StatsReporterInit() {
  DiskStatsReadStats(&read_sectors_, &write_sectors_);
  VmStatsReadStats(&vmstats_);
  // The first time around just run the long stat, so we don't delay boot.
  stats_state_ = kStatsLong;
  stats_initial_time_ = GetActiveTime();
  if (stats_initial_time_ < 0) {
    LOG(WARNING) << "not collecting disk stats";
  } else {
    ScheduleStatsCallback(kMetricStatsLongInterval);
  }
}

void MetricsDaemon::ScheduleStatsCallback(int wait) {
  if (testing_) {
    return;
  }
  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&MetricsDaemon::StatsCallback, base::Unretained(this)),
      base::TimeDelta::FromSeconds(wait));
}

bool MetricsDaemon::DiskStatsReadStats(uint64_t* read_sectors,
                                       uint64_t* write_sectors) {
  int nchars;
  int nitems;
  bool success = false;
  char line[200];
  if (diskstats_path_.empty()) {
    return false;
  }
  int file = HANDLE_EINTR(open(diskstats_path_.c_str(), O_RDONLY));
  if (file < 0) {
    PLOG(WARNING) << "cannot open " << diskstats_path_;
    return false;
  }
  nchars = HANDLE_EINTR(read(file, line, sizeof(line)));
  if (nchars < 0) {
    PLOG(WARNING) << "cannot read from " << diskstats_path_;
    return false;
  } else {
    LOG_IF(WARNING, nchars == sizeof(line))
        << "line too long in " << diskstats_path_;
    line[nchars] = '\0';
    nitems = sscanf(line, "%*d %*d %" PRIu64 " %*d %*d %*d %" PRIu64,
                    read_sectors, write_sectors);
    if (nitems == 2) {
      success = true;
    } else {
      LOG(WARNING) << "found " << nitems << " items in "
                   << diskstats_path_ << ", expected 2";
    }
  }
  IGNORE_EINTR(close(file));
  return success;
}

bool MetricsDaemon::VmStatsParseStats(const char* stats,
                                      struct VmstatRecord* record) {
  // a mapping of string name to field in VmstatRecord and whether we found it
  struct mapping {
    const string name;
    uint64_t* value_p;
    bool found;
  } map[] =
      { { .name = "pgmajfault",
          .value_p = &record->page_faults_,
          .found = false },
        { .name = "pswpin",
          .value_p = &record->swap_in_,
          .found = false },
        { .name = "pswpout",
          .value_p = &record->swap_out_,
          .found = false }, };

  // Each line in the file has the form
  // <ID> <VALUE>
  // for instance:
  // nr_free_pages 213427
  vector<string> lines = base::SplitString(stats, "\n", base::KEEP_WHITESPACE,
                                           base::SPLIT_WANT_NONEMPTY);
  for (vector<string>::iterator it = lines.begin();
       it != lines.end(); ++it) {
    vector<string> tokens = base::SplitString(*it, " ", base::KEEP_WHITESPACE,
                                              base::SPLIT_WANT_ALL);
    if (tokens.size() == 2) {
      for (unsigned int i = 0; i < sizeof(map)/sizeof(struct mapping); i++) {
        if (!tokens[0].compare(map[i].name)) {
          if (!base::StringToUint64(tokens[1], map[i].value_p))
            return false;
          map[i].found = true;
        }
      }
    } else {
      LOG(WARNING) << "unexpected vmstat format";
    }
  }
  // make sure we got all the stats
  for (unsigned i = 0; i < sizeof(map)/sizeof(struct mapping); i++) {
    if (map[i].found == false) {
      LOG(WARNING) << "vmstat missing " << map[i].name;
      return false;
    }
  }
  return true;
}

bool MetricsDaemon::VmStatsReadStats(struct VmstatRecord* stats) {
  string value_string;
  FilePath* path = new FilePath(vmstats_path_);
  if (!base::ReadFileToString(*path, &value_string)) {
    delete path;
    LOG(WARNING) << "cannot read " << vmstats_path_;
    return false;
  }
  delete path;
  return VmStatsParseStats(value_string.c_str(), stats);
}

bool MetricsDaemon::ReadFreqToInt(const string& sysfs_file_name, int* value) {
  const FilePath sysfs_path(sysfs_file_name);
  string value_string;
  if (!base::ReadFileToString(sysfs_path, &value_string)) {
    LOG(WARNING) << "cannot read " << sysfs_path.value().c_str();
    return false;
  }
  if (!base::RemoveChars(value_string, "\n", &value_string)) {
    LOG(WARNING) << "no newline in " << value_string;
    // Continue even though the lack of newline is suspicious.
  }
  if (!base::StringToInt(value_string, value)) {
    LOG(WARNING) << "cannot convert " << value_string << " to int";
    return false;
  }
  return true;
}

void MetricsDaemon::SendCpuThrottleMetrics() {
  // |max_freq| is 0 only the first time through.
  static int max_freq = 0;
  if (max_freq == -1)
    // Give up, as sysfs did not report max_freq correctly.
    return;
  if (max_freq == 0 || testing_) {
    // One-time initialization of max_freq.  (Every time when testing.)
    if (!ReadFreqToInt(cpuinfo_max_freq_path_, &max_freq)) {
      max_freq = -1;
      return;
    }
    if (max_freq == 0) {
      LOG(WARNING) << "sysfs reports 0 max CPU frequency\n";
      max_freq = -1;
      return;
    }
    if (max_freq % 10000 == 1000) {
      // Special case: system has turbo mode, and max non-turbo frequency is
      // max_freq - 1000.  This relies on "normal" (non-turbo) frequencies
      // being multiples of (at least) 10 MHz.  Although there is no guarantee
      // of this, it seems a fairly reasonable assumption.  Otherwise we should
      // read scaling_available_frequencies, sort the frequencies, compare the
      // two highest ones, and check if they differ by 1000 (kHz) (and that's a
      // hack too, no telling when it will change).
      max_freq -= 1000;
    }
  }
  int scaled_freq = 0;
  if (!ReadFreqToInt(scaling_max_freq_path_, &scaled_freq))
    return;
  // Frequencies are in kHz.  If scaled_freq > max_freq, turbo is on, but
  // scaled_freq is not the actual turbo frequency.  We indicate this situation
  // with a 101% value.
  int percent = scaled_freq > max_freq ? 101 : scaled_freq / (max_freq / 100);
  SendLinearSample(kMetricScaledCpuFrequencyName, percent, 101, 102);
}

// Collects disk and vm stats alternating over a short and a long interval.

void MetricsDaemon::StatsCallback() {
  uint64_t read_sectors_now, write_sectors_now;
  struct VmstatRecord vmstats_now;
  double time_now = GetActiveTime();
  double delta_time = time_now - stats_initial_time_;
  if (testing_) {
    // Fake the time when testing.
    delta_time = stats_state_ == kStatsShort ?
        kMetricStatsShortInterval : kMetricStatsLongInterval;
  }
  bool diskstats_success = DiskStatsReadStats(&read_sectors_now,
                                              &write_sectors_now);
  int delta_read = read_sectors_now - read_sectors_;
  int delta_write = write_sectors_now - write_sectors_;
  int read_sectors_per_second = delta_read / delta_time;
  int write_sectors_per_second = delta_write / delta_time;
  bool vmstats_success = VmStatsReadStats(&vmstats_now);
  uint64_t delta_faults = vmstats_now.page_faults_ - vmstats_.page_faults_;
  uint64_t delta_swap_in = vmstats_now.swap_in_ - vmstats_.swap_in_;
  uint64_t delta_swap_out = vmstats_now.swap_out_ - vmstats_.swap_out_;
  uint64_t page_faults_per_second = delta_faults / delta_time;
  uint64_t swap_in_per_second = delta_swap_in / delta_time;
  uint64_t swap_out_per_second = delta_swap_out / delta_time;

  switch (stats_state_) {
    case kStatsShort:
      if (diskstats_success) {
        SendSample(kMetricReadSectorsShortName,
                   read_sectors_per_second,
                   1,
                   kMetricSectorsIOMax,
                   kMetricSectorsBuckets);
        SendSample(kMetricWriteSectorsShortName,
                   write_sectors_per_second,
                   1,
                   kMetricSectorsIOMax,
                   kMetricSectorsBuckets);
      }
      if (vmstats_success) {
        SendSample(kMetricPageFaultsShortName,
                   page_faults_per_second,
                   1,
                   kMetricPageFaultsMax,
                   kMetricPageFaultsBuckets);
        SendSample(kMetricSwapInShortName,
                   swap_in_per_second,
                   1,
                   kMetricPageFaultsMax,
                   kMetricPageFaultsBuckets);
        SendSample(kMetricSwapOutShortName,
                   swap_out_per_second,
                   1,
                   kMetricPageFaultsMax,
                   kMetricPageFaultsBuckets);
      }
      // Schedule long callback.
      stats_state_ = kStatsLong;
      ScheduleStatsCallback(kMetricStatsLongInterval -
                            kMetricStatsShortInterval);
      break;
    case kStatsLong:
      if (diskstats_success) {
        SendSample(kMetricReadSectorsLongName,
                   read_sectors_per_second,
                   1,
                   kMetricSectorsIOMax,
                   kMetricSectorsBuckets);
        SendSample(kMetricWriteSectorsLongName,
                   write_sectors_per_second,
                   1,
                   kMetricSectorsIOMax,
                   kMetricSectorsBuckets);
        // Reset sector counters.
        read_sectors_ = read_sectors_now;
        write_sectors_ = write_sectors_now;
      }
      if (vmstats_success) {
        SendSample(kMetricPageFaultsLongName,
                   page_faults_per_second,
                   1,
                   kMetricPageFaultsMax,
                   kMetricPageFaultsBuckets);
        SendSample(kMetricSwapInLongName,
                   swap_in_per_second,
                   1,
                   kMetricPageFaultsMax,
                   kMetricPageFaultsBuckets);
        SendSample(kMetricSwapOutLongName,
                   swap_out_per_second,
                   1,
                   kMetricPageFaultsMax,
                   kMetricPageFaultsBuckets);

        vmstats_ = vmstats_now;
      }
      SendCpuThrottleMetrics();
      // Set start time for new cycle.
      stats_initial_time_ = time_now;
      // Schedule short callback.
      stats_state_ = kStatsShort;
      ScheduleStatsCallback(kMetricStatsShortInterval);
      break;
    default:
      LOG(FATAL) << "Invalid stats state";
  }
}

void MetricsDaemon::ScheduleMeminfoCallback(int wait) {
  if (testing_) {
    return;
  }
  base::TimeDelta waitDelta = base::TimeDelta::FromSeconds(wait);
  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&MetricsDaemon::MeminfoCallback, base::Unretained(this),
                 waitDelta),
      waitDelta);
}

void MetricsDaemon::MeminfoCallback(base::TimeDelta wait) {
  string meminfo_raw;
  const FilePath meminfo_path("/proc/meminfo");
  if (!base::ReadFileToString(meminfo_path, &meminfo_raw)) {
    LOG(WARNING) << "cannot read " << meminfo_path.value().c_str();
    return;
  }
  // Make both calls even if the first one fails.  Only stop rescheduling if
  // both calls fail, since some platforms do not support zram.
  bool success = ProcessMeminfo(meminfo_raw);
  bool reschedule =
      ReportZram(base::FilePath(FILE_PATH_LITERAL("/sys/block/zram0"))) ||
      success;
  if (reschedule) {
    base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&MetricsDaemon::MeminfoCallback, base::Unretained(this),
                   wait),
        wait);
  }
}

// static
bool MetricsDaemon::ReadFileToUint64(const base::FilePath& path,
                                     uint64_t* value) {
  std::string content;
  if (!base::ReadFileToString(path, &content)) {
    PLOG(WARNING) << "cannot read " << path.MaybeAsASCII();
    return false;
  }
  // Remove final newline.
  base::TrimWhitespaceASCII(content, base::TRIM_TRAILING, &content);
  if (!base::StringToUint64(content, value)) {
    LOG(WARNING) << "invalid integer: " << content;
    return false;
  }
  return true;
}

bool MetricsDaemon::ReportZram(const base::FilePath& zram_dir) {
  if (!base::DirectoryExists(zram_dir)) {
    return false;
  }

  // Data sizes are in bytes.  |zero_pages| is in number of pages.
  uint64_t compr_data_size, orig_data_size, zero_pages;
  const size_t page_size = 4096;

  if (!ReadFileToUint64(zram_dir.Append(kComprDataSizeName),
                        &compr_data_size) ||
      !ReadFileToUint64(zram_dir.Append(kOrigDataSizeName), &orig_data_size) ||
      !ReadFileToUint64(zram_dir.Append(kZeroPagesName), &zero_pages)) {
    return false;
  }

  // |orig_data_size| does not include zero-filled pages.
  orig_data_size += zero_pages * page_size;

  const int compr_data_size_mb = compr_data_size >> 20;
  const int savings_mb = (orig_data_size - compr_data_size) >> 20;

  // Report compressed size in megabytes.  100 MB or less has little impact.
  SendSample("Platform.ZramCompressedSize", compr_data_size_mb, 100, 4000, 50);
  SendSample("Platform.ZramSavings", savings_mb, 100, 4000, 50);
  // The compression ratio is multiplied by 100 for better resolution.  The
  // ratios of interest are between 1 and 6 (100% and 600% as reported).  We
  // don't want samples when very little memory is being compressed.
  //
  // A race in older versions of zram can make orig_data_size underflow and
  // be reported as a large positive number, so we also need to ensure that
  // orig_data_size multiplied by 100 isn't going to overflow.
  if (compr_data_size_mb >= 1 &&
      orig_data_size < (1ull << (sizeof(orig_data_size) * 8 - 1)) / 100) {
    SendSample("Platform.ZramCompressionRatioPercent",
               orig_data_size * 100 / compr_data_size, 100, 600, 50);
  }
  // The values of interest for zero_pages are between 1MB and 1GB.  The units
  // are number of pages.
  SendSample("Platform.ZramZeroPages", zero_pages, 256, 256 * 1024, 50);
  // Send ratio sample only when the ratio exists.
  if (orig_data_size > 0) {
    const int zero_percent = zero_pages * page_size * 100 / orig_data_size;
    SendSample("Platform.ZramZeroRatioPercent", zero_percent, 1, 50, 50);
  }

  return true;
}

bool MetricsDaemon::ProcessMeminfo(const string& meminfo_raw) {
  static const MeminfoRecord fields_array[] = {
    { "MemTotal", "MemTotal" },  // SPECIAL CASE: total system memory
    { "MemFree", "MemFree" },
    { "Buffers", "Buffers" },
    { "Cached", "Cached" },
    // { "SwapCached", "SwapCached" },
    { "Active", "Active" },
    { "Inactive", "Inactive" },
    { "ActiveAnon", "Active(anon)" },
    { "InactiveAnon", "Inactive(anon)" },
    { "ActiveFile" , "Active(file)" },
    { "InactiveFile", "Inactive(file)" },
    { "Unevictable", "Unevictable", kMeminfoOp_HistLog },
    // { "Mlocked", "Mlocked" },
    { "SwapTotal", "SwapTotal", kMeminfoOp_SwapTotal },
    { "SwapFree", "SwapFree", kMeminfoOp_SwapFree },
    // { "Dirty", "Dirty" },
    // { "Writeback", "Writeback" },
    { "AnonPages", "AnonPages" },
    { "Mapped", "Mapped" },
    { "Shmem", "Shmem", kMeminfoOp_HistLog },
    { "Slab", "Slab", kMeminfoOp_HistLog },
    // { "SReclaimable", "SReclaimable" },
    // { "SUnreclaim", "SUnreclaim" },
  };
  vector<MeminfoRecord> fields(fields_array,
                               fields_array + arraysize(fields_array));
  if (!FillMeminfo(meminfo_raw, &fields)) {
    return false;
  }
  int total_memory = fields[0].value;
  if (total_memory == 0) {
    // this "cannot happen"
    LOG(WARNING) << "borked meminfo parser";
    return false;
  }
  int swap_total = 0;
  int swap_free = 0;
  int mem_free_derived = 0;  // free + cached + buffers
  int mem_used_derived = 0;  // total - free_derived
  // Send all fields retrieved, except total memory.
  for (unsigned int i = 1; i < fields.size(); i++) {
    string metrics_name = base::StringPrintf("Platform.Meminfo%s",
                                             fields[i].name);
    int percent;
    switch (fields[i].op) {
      case kMeminfoOp_HistPercent:
        // report value as percent of total memory
        percent = fields[i].value * 100 / total_memory;
        SendLinearSample(metrics_name, percent, 100, 101);
        break;
      case kMeminfoOp_HistLog:
        // report value in kbytes, log scale, 4Gb max
        SendSample(metrics_name, fields[i].value, 1, 4 * 1000 * 1000, 100);
        break;
      case kMeminfoOp_SwapTotal:
        swap_total = fields[i].value;
        break;
      case kMeminfoOp_SwapFree:
        swap_free = fields[i].value;
        break;
    }
    if (strcmp(fields[i].match, "MemFree") == 0 ||
        strcmp(fields[i].match, "Buffers") == 0 ||
        strcmp(fields[i].match, "Cached") == 0) {
        mem_free_derived += fields[i].value;
    }
  }
  if (swap_total > 0) {
    int swap_used = swap_total - swap_free;
    int swap_used_percent = swap_used * 100 / swap_total;
    SendSample("Platform.MeminfoSwapUsed", swap_used, 1, 8 * 1000 * 1000, 100);
    SendLinearSample("Platform.MeminfoSwapUsedPercent", swap_used_percent,
                     100, 101);
  }
  mem_used_derived = total_memory - mem_free_derived;
  SendSample("Platform.MeminfoMemFreeDerived", mem_free_derived, 1,
             kMaximumMemorySizeInKB, 100);
  SendSample("Platform.MeminfoMemUsedDerived", mem_used_derived, 1,
             kMaximumMemorySizeInKB, 100);
  SendSample("Platform.MeminfoMemTotal", total_memory, 1,
             kMaximumMemorySizeInKB, 100);
  return true;
}

bool MetricsDaemon::FillMeminfo(const string& meminfo_raw,
                                vector<MeminfoRecord>* fields) {
  vector<string> lines =
      base::SplitString(meminfo_raw, "\n", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  // Scan meminfo output and collect field values.  Each field name has to
  // match a meminfo entry (case insensitive) after removing non-alpha
  // characters from the entry.
  size_t ifield = 0;
  for (size_t iline = 0;
       iline < lines.size() && ifield < fields->size();
       iline++) {
    vector<string> tokens =
        base::SplitString(lines[iline], ": ", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (strcmp((*fields)[ifield].match, tokens[0].c_str()) == 0) {
      // Name matches. Parse value and save.
      char* rest;
      (*fields)[ifield].value =
          static_cast<int>(strtol(tokens[1].c_str(), &rest, 10));
      if (*rest != '\0') {
        LOG(WARNING) << "missing meminfo value";
        return false;
      }
      ifield++;
    }
  }
  if (ifield < fields->size()) {
    // End of input reached while scanning.
    LOG(WARNING) << "cannot find field " << (*fields)[ifield].match
                 << " and following";
    return false;
  }
  return true;
}

void MetricsDaemon::ScheduleMemuseCallback(double interval) {
  if (testing_) {
    return;
  }
  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&MetricsDaemon::MemuseCallback, base::Unretained(this)),
      base::TimeDelta::FromSeconds(interval));
}

void MetricsDaemon::MemuseCallback() {
  // Since we only care about active time (i.e. uptime minus sleep time) but
  // the callbacks are driven by real time (uptime), we check if we should
  // reschedule this callback due to intervening sleep periods.
  double now = GetActiveTime();
  // Avoid intervals of less than one second.
  double remaining_time = ceil(memuse_final_time_ - now);
  if (remaining_time > 0) {
    ScheduleMemuseCallback(remaining_time);
  } else {
    // Report stats and advance the measurement interval unless there are
    // errors or we've completed the last interval.
    if (MemuseCallbackWork() &&
        memuse_interval_index_ < arraysize(kMemuseIntervals)) {
      double interval = kMemuseIntervals[memuse_interval_index_++];
      memuse_final_time_ = now + interval;
      ScheduleMemuseCallback(interval);
    }
  }
}

bool MetricsDaemon::MemuseCallbackWork() {
  string meminfo_raw;
  const FilePath meminfo_path("/proc/meminfo");
  if (!base::ReadFileToString(meminfo_path, &meminfo_raw)) {
    LOG(WARNING) << "cannot read " << meminfo_path.value().c_str();
    return false;
  }
  return ProcessMemuse(meminfo_raw);
}

bool MetricsDaemon::ProcessMemuse(const string& meminfo_raw) {
  static const MeminfoRecord fields_array[] = {
    { "MemTotal", "MemTotal" },  // SPECIAL CASE: total system memory
    { "ActiveAnon", "Active(anon)" },
    { "InactiveAnon", "Inactive(anon)" },
  };
  vector<MeminfoRecord> fields(fields_array,
                               fields_array + arraysize(fields_array));
  if (!FillMeminfo(meminfo_raw, &fields)) {
    return false;
  }
  int total = fields[0].value;
  int active_anon = fields[1].value;
  int inactive_anon = fields[2].value;
  if (total == 0) {
    // this "cannot happen"
    LOG(WARNING) << "borked meminfo parser";
    return false;
  }
  string metrics_name = base::StringPrintf("Platform.MemuseAnon%d",
                                           memuse_interval_index_);
  SendLinearSample(metrics_name, (active_anon + inactive_anon) * 100 / total,
                   100, 101);
  return true;
}

void MetricsDaemon::SendSample(const string& name, int sample,
                               int min, int max, int nbuckets) {
  metrics_lib_->SendToUMA(name, sample, min, max, nbuckets);
}

void MetricsDaemon::SendKernelCrashesCumulativeCountStats() {
  // Report the number of crashes for this OS version, but don't clear the
  // counter.  It is cleared elsewhere on version change.
  int64_t crashes_count = kernel_crashes_version_count_->Get();
  SendSample(kernel_crashes_version_count_->Name(),
             crashes_count,
             1,                         // value of first bucket
             500,                       // value of last bucket
             100);                      // number of buckets


  int64_t cpu_use_ms = version_cumulative_cpu_use_->Get();
  SendSample(version_cumulative_cpu_use_->Name(),
             cpu_use_ms / 1000,         // stat is in seconds
             1,                         // device may be used very little...
             8 * 1000 * 1000,           // ... or a lot (a little over 90 days)
             100);

  // On the first run after an autoupdate, cpu_use_ms and active_use_seconds
  // can be zero.  Avoid division by zero.
  if (cpu_use_ms > 0) {
    // Send the crash frequency since update in number of crashes per CPU year.
    SendSample("Platform.KernelCrashesPerCpuYear",
               crashes_count * kSecondsPerDay * 365 * 1000 / cpu_use_ms,
               1,
               1000 * 1000,     // about one crash every 30s of CPU time
               100);
  }

  int64_t active_use_seconds = version_cumulative_active_use_->Get();
  if (active_use_seconds > 0) {
    SendSample(version_cumulative_active_use_->Name(),
               active_use_seconds,
               1,                          // device may be used very little...
               8 * 1000 * 1000,            // ... or a lot (about 90 days)
               100);
    // Same as above, but per year of active time.
    SendSample("Platform.KernelCrashesPerActiveYear",
               crashes_count * kSecondsPerDay * 365 / active_use_seconds,
               1,
               1000 * 1000,     // about one crash every 30s of active time
               100);
  }
}

void MetricsDaemon::SendAndResetDailyUseSample(
    const std::unique_ptr<PersistentInteger>& use) {
  SendSample(use->Name(),
             use->GetAndClear(),
             1,                        // value of first bucket
             kSecondsPerDay,           // value of last bucket
             50);                      // number of buckets
}

void MetricsDaemon::SendAndResetCrashIntervalSample(
    const std::unique_ptr<PersistentInteger>& interval) {
  SendSample(interval->Name(),
             interval->GetAndClear(),
             1,                        // value of first bucket
             4 * kSecondsPerWeek,      // value of last bucket
             50);                      // number of buckets
}

void MetricsDaemon::SendAndResetCrashFrequencySample(
    const std::unique_ptr<PersistentInteger>& frequency) {
  SendSample(frequency->Name(),
             frequency->GetAndClear(),
             1,                        // value of first bucket
             100,                      // value of last bucket
             50);                      // number of buckets
}

void MetricsDaemon::SendLinearSample(const string& name, int sample,
                                     int max, int nbuckets) {
  // TODO(semenzato): add a proper linear histogram to the Chrome external
  // metrics API.
  LOG_IF(FATAL, nbuckets != max + 1) << "unsupported histogram scale";
  metrics_lib_->SendEnumToUMA(name, sample, max);
}

void MetricsDaemon::SendCroutonStats() {
  // Report the presence of /run/crouton: we only report each state
  // exactly once per boot ("0" state reported on init).
  if (PathExists(FilePath("/run/crouton"))) {
    SendLinearSample(kMetricCroutonStarted, 1, 2, 3);
  } else {
    base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&MetricsDaemon::SendCroutonStats,
                 base::Unretained(this)),
                 base::TimeDelta::FromMilliseconds(kUpdateStatsIntervalMs));
  }
}

void MetricsDaemon::UpdateStats(TimeTicks now_ticks,
                                Time now_wall_time) {
  const int elapsed_seconds = (now_ticks - last_update_stats_time_).InSeconds();
  daily_active_use_->Add(elapsed_seconds);
  version_cumulative_active_use_->Add(elapsed_seconds);
  user_crash_interval_->Add(elapsed_seconds);
  kernel_crash_interval_->Add(elapsed_seconds);
  version_cumulative_cpu_use_->Add(GetIncrementalCpuUse().InMilliseconds());
  last_update_stats_time_ = now_ticks;

  const TimeDelta since_epoch = now_wall_time - Time::UnixEpoch();
  const int day = since_epoch.InDays();
  const int week = day / 7;

  if (daily_cycle_->Get() != day) {
    daily_cycle_->Set(day);
    SendAndResetDailyUseSample(daily_active_use_);
    SendAndResetCrashFrequencySample(any_crashes_daily_count_);
    SendAndResetCrashFrequencySample(user_crashes_daily_count_);
    SendAndResetCrashFrequencySample(kernel_crashes_daily_count_);
    SendAndResetCrashFrequencySample(unclean_shutdowns_daily_count_);
    SendKernelCrashesCumulativeCountStats();
  }

  if (weekly_cycle_->Get() != week) {
    weekly_cycle_->Set(week);
    SendAndResetCrashFrequencySample(any_crashes_weekly_count_);
    SendAndResetCrashFrequencySample(user_crashes_weekly_count_);
    SendAndResetCrashFrequencySample(kernel_crashes_weekly_count_);
    SendAndResetCrashFrequencySample(unclean_shutdowns_weekly_count_);
  }
}

void MetricsDaemon::HandleUpdateStatsTimeout() {
  UpdateStats(TimeTicks::Now(), Time::Now());
  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&MetricsDaemon::HandleUpdateStatsTimeout,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kUpdateStatsIntervalMs));
}
