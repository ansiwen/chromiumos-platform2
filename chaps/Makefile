# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for chaps.
#
# Setting LINUX_BUILD=1 will build chaps for a standard (non-Chrome OS) linux
# system.

LINUX_BUILD=1

CC=clang
CXX=clang++

# Pull in chromium os defaults
PWD ?= $(CURDIR)
OUT ?= $(PWD)/build-opt-local

include common.mk

ifeq ($(LINUX_BUILD), 1)
  PLATFORM = linux
else
  PLATFORM = chromeos
endif

PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp
PROTOC = protoc
INSTALL ?= install
INSTALL_DATA = $(INSTALL) -m 0644
BUILD_PAM_MODULE ?= $(LINUX_BUILD)
LIB_DIR ?= /usr/lib
PAM_MODULE_DIR ?= /lib/security

#PC_DEPS = dbus-c++-1 protobuf-lite openssl \
	libchrome-$(BASE_VER) libbrillo-$(BASE_VER)
PC_DEPS =
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CFLAGS += -I$(SRC) -I$(SRC)/.. -I$(SRC)/../../Chromium/src -I$(OUT) $(PC_CFLAGS) -DNDEBUG -fexceptions
CXXFLAGS += -I$(SRC) -I$(SRC)/.. -I$(SRC)/../../Chromium/src -I$(OUT) $(PC_CFLAGS) -DNDEBUG -std=gnu++11 -fexceptions -Wno-deprecated-declarations
LDLIBS += -L/usr/local/opt/openssl/lib -L/usr/local/lib $(PC_LIBS) -framework CoreFoundation -framework AppKit -framework Security -L$(SRC)/../../Chromium/src/out/Default/obj/base/ -lbase -lbase_static -L$(SRC)/../../Chromium/src/out//Default/obj/base/third_party/dynamic_annotations -ldynamic_annotations -lprotobuf -levent -lssl -lcrypto -lboost_system -lboost_chrono -lboost_thread-mt -lcpprest

CFLAGS += -DCOMPILER_GCC -DARCH_CPU_X86_FAMILY -DOS_POSIX -DWCHAR_T_IS_UTF32 -DOS_MACOSX -Wno-format-nonliteral
CXXFLAGS += -DCOMPILER_GCC -DARCH_CPU_X86_FAMILY -DOS_POSIX -DWCHAR_T_IS_UTF32 -DOS_MACOSX -Wno-format-nonliteral

# Test if libmemenv is available, and whether libsnappy is required to
# compile LevelDB.
MEMENV_LIB := $(call check_cxx,-lmemenv)
ifneq ($(MEMENV_LIB), -lmemenv)
  CPPFLAGS += -DNO_MEMENV
endif
LEVELDB_LIBS := $(call check_libs_cxx, \
	'\#include<leveldb/db.h>\nint main(){\
	 leveldb::Options options; leveldb::DB::Open(options, 0, 0);\
	 return 0;}',\
	 -lleveldb $(MEMENV_LIB), -lleveldb $(MEMENV_LIB) -lsnappy)

LDLIBS += $(LEVELDB_LIBS)

# Test if libmetric is available
METRICS_LIB := $(call check_cxx,-lmetrics)
ifneq ($(METRICS_LIB), -lmetrics)
  CPPFLAGS += -DNO_METRICS
endif

DBUS_ADAPTORS_DIR = $(OUT)/chaps/dbus_adaptors
DBUS_PROXIES_DIR = $(OUT)/chaps/dbus_proxies

# Rules for Generated Code
$(DBUS_PROXIES_DIR)/chaps_interface.h: $(SRC)/chaps_interface.xml
	mkdir -p $(DBUS_PROXIES_DIR)
	$(DBUSXX_XML2CPP) $< --proxy=$@
clean: CLEAN($(DBUS_PROXIES_DIR)/chaps_interface.h)

#chaps_client.o.depends \
chaps_proxy.o.depends \
chapsd_test.o.depends \
chaps.o.depends \
chaps_event_generator.o.depends \
token_manager_client.o.depends: $(DBUS_PROXIES_DIR)/chaps_interface.h

$(DBUS_ADAPTORS_DIR)/chaps_interface.h: $(SRC)/chaps_interface.xml
	mkdir -p $(DBUS_ADAPTORS_DIR)
	$(DBUSXX_XML2CPP) $< --adaptor=$@
clean: CLEAN($(DBUS_ADAPTORS_DIR)/chaps_interface.h)

chaps_adaptor.o.depends \
chapsd.o.depends : $(DBUS_ADAPTORS_DIR)/chaps_interface.h

PROTO_DIR = $(OUT)/chaps/proto_bindings

$(PROTO_DIR)/attributes.pb.h \
$(PROTO_DIR)/attributes.pb.cc: $(SRC)/attributes.proto
	mkdir -p $(PROTO_DIR)
	$(PROTOC) -I$(SRC) --cpp_out=$(OUT)/chaps/proto_bindings $<
clean: CLEAN($(PROTO_DIR)/attributes.pb.h $(PROTO_DIR)/attributes.pb.cc)
$(eval $(call add_object_rules,$(PROTO_DIR)/attributes.pb.o,CXX,cc,CXXFLAGS))

$(eval $(call add_object_rules,base/message_loop/message_pump_mac.o,CXX,mm,CXXFLAGS,$(SRC)/))
object_pool_impl.o.depends \
object_pool_test.o.depends \
attributes.o.depends: $(PROTO_DIR)/attributes.pb.h

# Common Files
COMMON_OBJS = chaps_utility.o $(PROTO_DIR)/attributes.pb.o attributes.o
#COMMON_OBJS = chaps_utility.o attributes.o

# Chaps Daemon
chapsd_OBJS = $(COMMON_OBJS) \
              chapsd.o \
              chaps_service.o \
              chaps_service_redirect.o \
              chaps_adaptor.o \
              isolate_$(PLATFORM).o \
              slot_manager_impl.o \
              session_impl.o \
              object_impl.o \
              object_policy_common.o \
              object_policy_data.o \
              object_policy_cert.o \
              object_policy_key.o \
              object_policy_public_key.o \
              object_policy_private_key.o \
              object_policy_secret_key.o \
              object_pool_impl.o \
              platform_globals_$(PLATFORM).o \
              tpm_utility_impl.o \
              chaps_factory_impl.o \
              object_store_impl.o \
              opencryptoki_importer.o
chapsd_LIBS = -ldl -ltspi $(LEVELDB_LIBS) $(METRICS_LIB)
CXX_BINARY(chapsd): $(chapsd_OBJS)
CXX_BINARY(chapsd): LDLIBS += $(chapsd_LIBS)
clean: CLEAN(chapsd)
all: CXX_BINARY(chapsd)

# Chaps DBus Configuration
ifeq ($(LINUX_BUILD),1)
DBUS_POLICY = "context=\"default\""
else
DBUS_POLICY = "group=\"pkcs11\""
endif
DBUS_FLAGS := $(call check_compile_cxx, \
	'\#include <dbus-c++/dbus.h>\nint main(){\
	DBus::Connection::SystemBus().acquire_name("dummy");\
	return 0;}',,-DNO_DBUS_ACQUIRE_NAME)
CXXFLAGS += $(DBUS_FLAGS)

.PHONY: $(OUT)/org.chromium.Chaps.conf
$(OUT)/org.chromium.Chaps.conf :
	sed -e "s/@POLICY_PERMISSIONS@/$(DBUS_POLICY)/" \
               $(SRC)/org.chromium.Chaps.conf.in > $@
all: $(OUT)/org.chromium.Chaps.conf
clean: CLEAN($(OUT)/org.chromium.Chaps.conf)

XXX_BASE_OBJS =     base/at_exit.o \
                base/base_switches.o \
                base/callback_internal.o \
                base/command_line.o \
                base/debug/alias.o \
                base/debug/debugger.o \
                base/debug/debugger_posix.o \
                base/debug/stack_trace.o \
                base/debug/stack_trace_posix.o \
                base/environment.o \
                base/files/file.o \
                base/files/file_enumerator.o \
                base/files/file_enumerator_posix.o \
                base/files/file_path.o \
                base/files/file_path_constants.o \
                base/files/file_posix.o \
                base/files/file_tracing.o \
                base/files/file_util.o \
                base/files/file_util_posix.o \
                base/files/scoped_file.o \
                base/files/scoped_temp_dir.o \
								base/hash.o \
                base/json/json_parser.o \
                base/json/json_reader.o \
                base/json/json_string_value_serializer.o \
                base/json/json_writer.o \
                base/json/string_escape.o \
                base/lazy_instance.o \
                base/location.o \
                base/logging.o \
                base/md5.o \
                base/memory/ref_counted.o \
                base/memory/ref_counted_memory.o \
                base/memory/singleton.o \
                base/memory/weak_ptr.o \
								base/memory/shared_memory_mac.o \
								base/message_loop/message_loop.o \
                base/message_loop/message_pump.o \
                base/message_loop/message_pump_default.o \
								base/message_loop/message_pump_mac.o \
								base/message_loop/message_pump_libevent.o \
                base/metrics/bucket_ranges.o \
                base/metrics/histogram_base.o \
                base/metrics/histogram_samples.o \
                base/metrics/histogram.o \
                base/metrics/metrics_hashes.o \
								base/metrics/persistent_sample_map.o \
                base/metrics/sample_map.o \
                base/metrics/sample_vector.o \
                base/metrics/sparse_histogram.o \
                base/metrics/statistics_recorder.o \
								base/metrics/persistent_histogram_allocator.o \
								base/pending_task.o \
                base/pickle.o \
                base/posix/safe_strerror.o \
								base/process/process_metrics.o \
								base/process/process_handle.o \
                base/process/process_handle_posix.o \
								base/process/process_metrics_mac.o \
								base/process/process_metrics_posix.o \
                base/profiler/tracked_time.o \
                base/rand_util.o \
                base/rand_util_posix.o \
								base/sequenced_task_runner.o \
                base/strings/string16.o \
                base/strings/string_piece.o \
                base/strings/stringprintf.o \
								base/strings/string_util.o \
                base/strings/string_util_constants.o \
                base/strings/string_split.o \
                base/strings/string_number_conversions.o \
                base/strings/sys_string_conversions_posix.o \
                base/strings/utf_string_conversions.o \
                base/strings/utf_string_conversion_utils.o \
                base/synchronization/condition_variable_posix.o \
                base/synchronization/lock.o \
                base/synchronization/lock_impl_posix.o \
                base/synchronization/waitable_event_posix.o \
                base/sys_info_posix.o \
								base/task_runner.o \
								base/task_scheduler/task_traits.o \
								base/task_scheduler/task_scheduler.o \
                base/third_party/dmg_fp/dtoa.o \
                base/third_party/dmg_fp/g_fmt.o \
                base/third_party/dynamic_annotations/dynamic_annotations.o \
                base/third_party/icu/icu_utf.o \
                base/third_party/nspr/prtime.o \
								base/third_party/superfasthash/superfasthash.o \
                base/threading/non_thread_safe_impl.o \
                base/threading/platform_thread_internal_posix.o \
                base/threading/platform_thread_posix.o \
								base/threading/platform_thread_freebsd.o \
								base/threading/sequenced_task_runner_handle.o \
								base/threading/sequenced_worker_pool.o \
								base/threading/simple_thread.o \
                base/threading/thread_id_name_manager.o \
                base/threading/thread_checker_impl.o \
                base/threading/thread_collision_warner.o \
                base/threading/thread_local_storage.o \
                base/threading/thread_local_storage_posix.o \
                base/threading/thread_restrictions.o \
								base/threading/thread_task_runner_handle.o \
								base/threading/worker_pool.o \
								base/threading/worker_pool_posix.o \
								base/trace_event/auto_open_close_event.o \
								base/trace_event/blame_context.o \
								base/trace_event/category_registry.o \
								base/trace_event/event_name_filter.o \
								base/trace_event/heap_profiler_allocation_context.o \
								base/trace_event/heap_profiler_allocation_context_tracker.o \
								base/trace_event/heap_profiler_allocation_register.o \
								base/trace_event/heap_profiler_allocation_register_posix.o \
								base/trace_event/heap_profiler_event_filter.o \
								base/trace_event/heap_profiler_heap_dump_writer.o \
								base/trace_event/heap_profiler_stack_frame_deduplicator.o \
								base/trace_event/heap_profiler_type_name_deduplicator.o \
								base/trace_event/malloc_dump_provider.o \
								base/trace_event/memory_allocator_dump.o \
								base/trace_event/memory_allocator_dump_guid.o \
								base/trace_event/memory_dump_manager.o \
								base/trace_event/memory_dump_request_args.o \
								base/trace_event/memory_dump_session_state.o \
								base/trace_event/memory_infra_background_whitelist.o \
								base/trace_event/memory_usage_estimator.o \
								base/trace_event/process_memory_dump.o \
								base/trace_event/process_memory_maps.o \
								base/trace_event/process_memory_totals.o \
								base/trace_event/trace_buffer.o \
								base/trace_event/trace_config.o \
								base/trace_event/trace_event_argument.o \
								base/trace_event/trace_event_filter.o \
								base/trace_event/trace_event_filter_test_utils.o \
								base/trace_event/trace_event_impl.o \
								base/trace_event/trace_event_memory_overhead.o \
								base/trace_event/trace_event_synthetic_delay.o \
								base/trace_event/trace_event_system_stats_monitor.o \
								base/trace_event/trace_log.o \
								base/trace_event/trace_log_constants.o \
								base/trace_event/tracing_agent.o \
								base/tracking_info.o \
                base/time/time.o \
                base/time/time_posix.o \
								base/time/time_mac.o \
                base/timer/elapsed_timer.o \
                base/tracked_objects.o \
								base/sha1.o \
								base/sequence_token.o \
                base/strings/utf_string_conversions.o \
								base/strings/pattern.o \
                base/values.o \
                base/vlog.o



# Chaps Client Library
libchaps_OBJS = $(COMMON_OBJS) chaps.o \
								chaps_service.o \
								slot_manager_impl.o \
								session_impl.o \
								object_impl.o \
								object_policy_common.o \
								object_policy_data.o \
								object_policy_cert.o \
								object_policy_key.o \
								object_policy_public_key.o \
								object_policy_private_key.o \
								object_policy_secret_key.o \
								object_pool_impl.o \
								chaps_factory_impl.o \
								object_store_impl.o \
								net_utility_impl.o \
								attributes.o \
								brillo/secure_blob.o
CXX_LIBRARY(libchaps.so): $(libchaps_OBJS)
clean: CLEAN(libchaps.so)
all: CXX_LIBRARY(libchaps.so)

ifneq ($(CHAPS_VERSION_MAJOR),)
ifneq ($(CHAPS_VERSION_MINOR),)
CHAPS_VERSION=$(CHAPS_VERSION_MAJOR).$(CHAPS_VERSION_MINOR)
# Also build a version with SONAME set.
CXX_LIBRARY(libchaps.so.$(CHAPS_VERSION)): $(libchaps_OBJS)
CXX_LIBRARY(libchaps.so.$(CHAPS_VERSION)): LDFLAGS += -Wl,-soname,libchaps.so.$(CHAPS_VERSION_MAJOR)
clean: CLEAN(libchaps.so.$(CHAPS_VERSION))
all: CXX_LIBRARY(libchaps.so.$(CHAPS_VERSION))
endif
endif

# Chaps Client CLI App
chaps_client_OBJS = $(COMMON_OBJS) chaps_client.o chaps_proxy.o \
                    token_manager_client.o isolate_$(PLATFORM).o
CXX_BINARY(chaps_client): $(chaps_client_OBJS)
clean: CLEAN(chaps_client)
all: CXX_BINARY(chaps_client)

# PKCS #11 Replay Utility
p11_replay_OBJS = p11_replay.o
CXX_BINARY(p11_replay): $(p11_replay_OBJS) CXX_LIBRARY(libchaps.so)
clean: CLEAN(p11_replay)
all: CXX_BINARY(p11_replay)

ifeq ($(BUILD_PAM_MODULE), 1)
# Chaps PAM Module
pamchaps_OBJS = $(COMMON_OBJS) chaps_pam_module.o chaps.o chaps_proxy.o \
                token_manager_client.o isolate_login_client.o pam_helper.o \
                token_file_manager_$(PLATFORM).o isolate_$(PLATFORM).o \
                platform_globals_$(PLATFORM).o
CXX_LIBRARY(pam_chaps.so): $(pamchaps_OBJS)
CXX_LIBRARY(pam_chaps.so): LDLIBS += -lpam
clean: CLEAN(pam_chaps.so)
all: CXX_LIBRARY(pam_chaps.so)
endif

ifeq ($(LINUX_BUILD), 1)
# Install chaps files into their locations on Linux.
install_files: all
	$(INSTALL) -D $(OUT)/chapsd $(DESTDIR)/usr/sbin/chapsd
	$(INSTALL) -D $(OUT)/chaps_client $(DESTDIR)/usr/bin/chaps_client
ifneq ($(CHAPS_VERSION),)
	$(INSTALL) -D $(OUT)/libchaps.so.$(CHAPS_VERSION) $(DESTDIR)$(LIB_DIR)/libchaps.so.$(CHAPS_VERSION)
	cd $(DESTDIR)$(LIB_DIR) && ln -s -f libchaps.so.$(CHAPS_VERSION) libchaps.so.$(CHAPS_VERSION_MAJOR)
	cd $(DESTDIR)$(LIB_DIR) && ln -s -f libchaps.so.$(CHAPS_VERSION_MAJOR) libchaps.so
else
	$(INSTALL) -D $(OUT)/libchaps.so $(DESTDIR)$(LIB_DIR)/libchaps.so
endif
	$(INSTALL_DATA) -D $(OUT)/org.chromium.Chaps.conf \
	  $(DESTDIR)/etc/dbus-1/system.d/org.chromium.Chaps.conf
	$(INSTALL_DATA) -D $(SRC)/org.chromium.Chaps.service \
	  $(DESTDIR)/usr/share/dbus-1/system-services/org.chromium.Chaps.service
ifeq ($(BUILD_PAM_MODULE), 1)
	$(INSTALL) -D $(OUT)/pam_chaps.so \
          $(DESTDIR)$(PAM_MODULE_DIR)/pam_chaps.so
	$(INSTALL_DATA) -D $(SRC)/pam_chaps.cfg \
	  $(DESTDIR)/usr/share/pam-configs/chaps
endif

# Perform all post-install operations for Chaps on Linux.
install_ops: install_files
	$(INSTALL) -m 700 -d $(DESTDIR)/var/lib/chaps/tokens/
	$(INSTALL) -m 755 -d $(DESTDIR)/var/lib/chaps/isolates/
ifeq ($(BUILD_PAM_MODULE), 1)
ifeq ($(DESTDIR),)
	pam-auth-update --package
endif
endif

install: install_files install_ops
endif

# Common Test Dependencies
MOCK_OBJS = chaps_factory_mock.o \
            object_mock.o \
            object_policy_mock.o \
            object_pool_mock.o \
            object_store_mock.o \
            pam_helper_mock.o \
            session_mock.o \
            slot_manager_mock.o \
            tpm_utility_mock.o \
            object_importer_mock.o

GMOCK_LIBS = -lgtest -lgmock

# QEMU will not run death-tests, generate RSA keys, or handle the import sample
# tarball. These tests need to be filtered out.
qemu_test_exclusions = *DeathTest* \
                       *ImportSample* \
                       TestSession.RSA* \
                       TestSession.KeyTypeMismatch \
                       TestSession.KeyFunctionPermission \
                       TestSession.BadKeySize \
                       TestSession.BadSignature
# We want $(space)=' '.
space :=
space +=
qemu_test_filter := -$(subst $(space),:,$(strip $(qemu_test_exclusions)))
tests: override GTEST_ARGS.qemu.arm := --gtest_filter=$(qemu_test_filter)

# Unit Tests
chaps_test_OBJS = $(COMMON_OBJS) isolate_$(PLATFORM).o chaps_test.o
chaps_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(chaps_test): $(chaps_test_OBJS) CXX_LIBRARY(libchaps.so)
CXX_BINARY(chaps_test): LDLIBS += $(chaps_test_LIBS)
clean: CLEAN(chaps_test)
tests: TEST(CXX_BINARY(chaps_test))

chaps_service_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                          chaps_service_test.o chaps_service.o \
                          isolate_$(PLATFORM).o
chaps_service_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(chaps_service_test): $(chaps_service_test_OBJS)
CXX_BINARY(chaps_service_test): LDLIBS += $(chaps_service_test_LIBS)
clean: CLEAN(chaps_service_test)
tests: TEST(CXX_BINARY(chaps_service_test))

slot_manager_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                         slot_manager_test.o slot_manager_impl.o \
                         isolate_$(PLATFORM).o
slot_manager_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(slot_manager_test): $(slot_manager_test_OBJS)
CXX_BINARY(slot_manager_test): LDLIBS += $(slot_manager_test_LIBS)
clean: CLEAN(slot_manager_test)
tests: TEST(CXX_BINARY(slot_manager_test))

session_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) session_test.o session_impl.o
session_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(session_test): $(session_test_OBJS)
CXX_BINARY(session_test): LDLIBS += $(session_test_LIBS)
clean: CLEAN(session_test)
tests: TEST(CXX_BINARY(session_test))

object_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) object_test.o object_impl.o
object_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(object_test): $(object_test_OBJS)
CXX_BINARY(object_test): LDLIBS += $(object_test_LIBS)
clean: CLEAN(object_test)
tests: TEST(CXX_BINARY(object_test))

object_policy_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                          object_policy_test.o \
                          object_policy_common.o \
                          object_policy_data.o \
                          object_policy_cert.o \
                          object_policy_key.o \
                          object_policy_public_key.o \
                          object_policy_private_key.o \
                          object_policy_secret_key.o
object_policy_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(object_policy_test): $(object_policy_test_OBJS)
CXX_BINARY(object_policy_test): LDLIBS += $(object_policy_test_LIBS)
clean: CLEAN(object_policy_test)
tests: TEST(CXX_BINARY(object_policy_test))

object_pool_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                        object_pool_test.o object_pool_impl.o
object_pool_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(object_pool_test): $(object_pool_test_OBJS)
CXX_BINARY(object_pool_test): LDLIBS += $(object_pool_test_LIBS)
clean: CLEAN(object_pool_test)
tests: TEST(CXX_BINARY(object_pool_test))

object_store_test_OBJS = $(COMMON_OBJS) object_store_test.o object_store_impl.o
object_store_test_LIBS = -lgtest $(LEVELDB_LIBS) $(METRICS_LIB)

CXX_BINARY(object_store_test): $(object_store_test_OBJS)
CXX_BINARY(object_store_test): LDLIBS += $(object_store_test_LIBS)
clean: CLEAN(object_store_test)
tests: TEST(CXX_BINARY(object_store_test))

opencryptoki_importer_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                                  opencryptoki_importer_test.o \
                                  opencryptoki_importer.o
opencryptoki_importer_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(opencryptoki_importer_test): $(opencryptoki_importer_test_OBJS)
CXX_BINARY(opencryptoki_importer_test): LDLIBS += \
                                        $(opencryptoki_importer_test_LIBS)
clean: CLEAN(opencryptoki_importer_test)
tests: $(OUT)/opencryptoki_sample_token.tgz
$(OUT)/opencryptoki_sample_token.tgz:
	cp $(SRC)/opencryptoki_sample_token.tgz $@
clean: CLEAN(opencryptoki_sample_token.tgz)
tests: TEST(CXX_BINARY(opencryptoki_importer_test))

isolate_login_client_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                                 isolate_$(PLATFORM).o \
                                 token_file_manager_$(PLATFORM).o \
                                 token_manager_client.o \
                                 chaps_proxy.o \
                                 isolate_login_client.o \
                                 isolate_login_client_test.o
isolate_login_client_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(isolate_login_client_test): $(isolate_login_client_test_OBJS)
CXX_BINARY(isolate_login_client_test): LDLIBS += \
                                       $(isolate_login_client_test_LIBS)
clean: CLEAN(isolate_login_client_test)
tests: TEST(CXX_BINARY(isolate_login_client_test))

ifeq ($(BUILD_PAM_MODULE), 1)
chaps_pam_module_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                             isolate_$(PLATFORM).o \
                             token_file_manager_$(PLATFORM).o \
                             platform_globals_$(PLATFORM).o \
                             token_manager_client.o \
                             isolate_login_client.o \
                             chaps_proxy.o \
                             pam_helper.o \
                             chaps_pam_module_test.o \
                             chaps_pam_module.o
chaps_pam_module_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(chaps_pam_module_test): $(chaps_pam_module_test_OBJS)
CXX_BINARY(chaps_pam_module_test): LDLIBS += \
                                   $(chaps_pam_module_test_LIBS)
clean: CLEAN(chaps_pam_module_test)
tests: TEST(CXX_BINARY(chaps_pam_module_test))
endif

import_random: override GTEST_ARGS := \
    --gtest_repeat=100 \
    --gtest_break_on_failure \
    --gtest_filter=RandomizedTests/TestImporterWithModifier.ImportSample/*
import_random: override GTEST_ARGS.qemu.arm := --gtest_filter=-*
import_random: TEST(CXX_BINARY(opencryptoki_importer_test))
more_tests: import_random

# Live Tests
# Note: These tests require a live system with gtest and gmock installed. These
# cannot be run without a real TPM and cannot be run with autotest. These tests
# do not need to be run regularly but may be useful in the future and so have
# been kept around. To run these tests:
# 1) Add the targets to 'all'.
# 2) Add 'dobin' instructions to the ebuild and install on target device.
# 3) On the target system, login as any user and open a shell.
# 4) Run 'sudo tpm_utility_test'.
# 5) Run 'sudo chapsd_test'.
# 6) Run 'chapsd_test --use_dbus'.
chapsd_test_OBJS = $(COMMON_OBJS) chapsd_test.o chaps_proxy.o \
                   chaps_service_redirect.o
chapsd_test_LIBS = -lgtest -ldl
CXX_BINARY(chapsd_test): $(chapsd_test_OBJS) CXX_LIBRARY(libchaps.so)
CXX_BINARY(chapsd_test): LDLIBS += $(chapsd_test_LIBS)
clean: CLEAN(chapsd_test)

tpm_utility_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                        tpm_utility_test.o \
                        tpm_utility_impl.o
tpm_utility_test_LIBS = $(GMOCK_LIBS) -ltspi
CXX_BINARY(tpm_utility_test): $(tpm_utility_test_OBJS)
CXX_BINARY(tpm_utility_test): LDLIBS += $(tpm_utility_test_LIBS)
clean: CLEAN(tpm_utility_test)
