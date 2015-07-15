// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/settings_document_manager.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>

#include <base/values.h>
#include <gtest/gtest.h>

#include "settingsd/mock_settings_document.h"
#include "settingsd/settings_keys.h"
#include "settingsd/simple_settings_map.h"
#include "settingsd/source.h"
#include "settingsd/source_delegate.h"
#include "settingsd/test_helpers.h"
#include "settingsd/version_stamp.h"

namespace settingsd {

namespace {

// Test constants.
const char kTestSource0[] = "source0";
const char kTestSource1[] = "source1";
const char kTestSource2[] = "source2";

const char kSharedKey[] = "shared";

// A SettingsObserver implementation that verifies expectations.
class SettingsChangeVerifier : public SettingsObserver {
 public:
  SettingsChangeVerifier(SettingsDocumentManager* manager,
                         const std::deque<std::set<Key>>& expectations)
      : manager_(manager), expectations_(expectations) {
    manager_->AddSettingsObserver(this);
  }

  ~SettingsChangeVerifier() {
    EXPECT_TRUE(expectations_.empty());
    manager_->RemoveSettingsObserver(this);
  }

  void OnSettingsChanged(const std::set<Key> keys) override {
    ASSERT_FALSE(expectations_.empty());
    EXPECT_TRUE(std::includes(keys.begin(), keys.end(),
                              expectations_.front().begin(),
                              expectations_.front().end()));
    expectations_.pop_front();
  }

 private:
  SettingsDocumentManager* manager_;
  std::deque<std::set<Key>> expectations_;

  DISALLOW_COPY_AND_ASSIGN(SettingsChangeVerifier);
};

std::unique_ptr<SourceDelegate> CreateDummySourceDelegate(
    const std::string& source_id,
    const SettingsService& settings) {
  return std::unique_ptr<SourceDelegate>(new DummySourceDelegate);
}

// Add trust configuration for |source_id| to |doc|.
static void ConfigureSource(MockSettingsDocument* doc,
                            const std::string& source_id,
                            SettingStatus status,
                            const std::map<Key, SettingStatus> access_rules) {
  doc->SetKey(MakeSourceKey(source_id).Extend({keys::sources::kStatus}),
              MakeStringValue(SettingStatusToString(status)));
  doc->SetKey(MakeSourceKey(source_id).Extend({keys::sources::kName}),
              MakeStringValue(source_id));
  for (auto& access_rule : access_rules) {
    doc->SetKey(MakeSourceKey(source_id)
                    .Extend({keys::sources::kAccess})
                    .Append(access_rule.first),
                MakeStringValue(SettingStatusToString(access_rule.second)));
  }
}

// Create the initial trusted settings document for bootstrapping
// SettingsDocumentManager. This configures a source that has access to its
// sentinel value as well as the trust configuration for other sources.
std::unique_ptr<SettingsDocument> CreateInitialTrustedSettingsDocument() {
  // No version stamp is fine, since the initial document can't collide with
  // anything and doesn't have an associated source for which it'd need to
  // supply a unique version component.
  std::unique_ptr<MockSettingsDocument> document(
      new MockSettingsDocument(VersionStamp()));
  ConfigureSource(document.get(), kTestSource0, kSettingStatusActive,
                  {{MakeSourceKey(kTestSource1), kSettingStatusActive},
                   {MakeSourceKey(kTestSource2), kSettingStatusActive}});
  return std::move(document);
}

}  // namespace

class SettingsDocumentManagerTest : public testing::Test {
 public:
  SettingsDocumentManagerTest()
      : manager_(CreateDummySourceDelegate,
                 std::unique_ptr<SettingsMap>(new SimpleSettingsMap()),
                 CreateInitialTrustedSettingsDocument()) {}

  // Wrapper for SettingsDocumentManager::InsertDocument, which is private.
  SettingsDocumentManager::InsertionStatus InsertDocument(
      std::unique_ptr<SettingsDocument> document,
      const std::string& source_id,
      const std::deque<std::set<Key>>& expected_changes) {
    SettingsChangeVerifier verifier(&manager_, expected_changes);
    return manager_.InsertDocument(std::move(document), source_id);
  }

  // Creates a settings document with a bumped version stamp.
  std::unique_ptr<MockSettingsDocument> MakeDocument(
      const std::string& source_id) {
    current_version_.Set(source_id, current_version_.Get(source_id) + 1);
    return std::unique_ptr<MockSettingsDocument>(
        new MockSettingsDocument(current_version_));
  }

  // A helper to configure a trusted source.
  void ConfigureTrustedSource(const std::string& added_source) {
    SCOPED_TRACE("Configuring source " + added_source);
    std::unique_ptr<MockSettingsDocument> document(
        MakeDocument(kTestSource0));
    ConfigureSource(document.get(), added_source, kSettingStatusActive,
                    {{Key(added_source), kSettingStatusActive},
                     {Key(kSharedKey), kSettingStatusActive}});
    EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
              InsertDocument(std::move(document), kTestSource0, {{}}));
  }

  // A helper that sets a settings key serving as the sentinel value for whether
  // the source it originates from is still valid.
  void AddSentinelValue(const std::string& source_id) {
    SCOPED_TRACE("Adding sentinel value for " + source_id);
    std::unique_ptr<MockSettingsDocument> document(MakeDocument(source_id));
    document->SetKey(Key(source_id), MakeStringValue(source_id));
    EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
              InsertDocument(std::move(document), source_id, {{}}));
  }

  // Checks presence of the correct sentinel values.
  void CheckSentinelValues(std::initializer_list<std::string> present,
                           std::initializer_list<std::string> absent) {
    for (auto& source : present) {
      const base::Value* value = manager_.GetValue(Key(source));
      EXPECT_TRUE(value) << "Sentinel value " << source << " missing.";
      base::StringValue sentinel_value(source);
      EXPECT_TRUE(base::Value::Equals(&sentinel_value, value))
          << "Sentinel value " << source << " has wrong value.";
    }

    for (auto& source : absent) {
      const base::Value* value = manager_.GetValue(Key(source));
      EXPECT_FALSE(value) << "Sentinel value " << source << " present.";
    }
  }

  VersionStamp current_version_;
  SettingsDocumentManager manager_;
};

TEST_F(SettingsDocumentManagerTest, ValueInsertionAndRemoval) {
  ConfigureTrustedSource(kTestSource1);
  const Key kTestKey(kTestSource1);

  // Insert a document with a fresh key.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource1));
  document->SetKey(kTestKey, MakeIntValue(42));
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource1, {{kTestKey}}));
  const base::Value* value = manager_.GetValue(kTestKey);
  base::FundamentalValue expected_int_value(42);
  EXPECT_TRUE(base::Value::Equals(&expected_int_value, value));

  // Update the value.
  document = MakeDocument(kTestSource1);
  document->SetKey(kTestKey, MakeStringValue("string"));
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource1, {{kTestKey}}));
  value = manager_.GetValue(kTestKey);
  base::StringValue expected_string_value("string");
  EXPECT_TRUE(base::Value::Equals(&expected_string_value, value));

  // Clear the value.
  document = MakeDocument(kTestSource1);
  document->SetDeletion(kTestKey);
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource1, {{kTestKey}}));
  value = manager_.GetValue(kTestKey);
  EXPECT_FALSE(value);
}

TEST_F(SettingsDocumentManagerTest, TrustChange) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);

  // Check whether the sentinel for the inserted source is present.
  CheckSentinelValues({kTestSource1}, {});

  // Remove trust, which should make the sentinel value disappear.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  ConfigureSource(document.get(), kTestSource1, kSettingStatusInvalid, {});
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusSuccess,
      InsertDocument(std::move(document), kTestSource0, {{Key(kTestSource1)}}));
  CheckSentinelValues({}, {kTestSource1});
}

TEST_F(SettingsDocumentManagerTest, CascadingRemoval) {
  // Have source0 add source1 and grant it access to source2.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  ConfigureSource(document.get(), kTestSource1, kSettingStatusActive,
                  {{Key(kTestSource1), kSettingStatusActive},
                   {MakeSourceKey(kTestSource2), kSettingStatusActive}});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource0, {{}}));
  AddSentinelValue(kTestSource1);

  // Have source1 extend trust to source2.
  document = MakeDocument(kTestSource1);
  ConfigureSource(document.get(), kTestSource2, kSettingStatusActive,
                  {{Key(kTestSource2), kSettingStatusActive}});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource1, {{}}));
  AddSentinelValue(kTestSource2);

  // Both sentinels should be present.
  CheckSentinelValues({kTestSource1, kTestSource2}, {});

  // Revoke trust from kTestSource1. kTestSource2 becomes invalid as well.
  document = MakeDocument(kTestSource0);
  ConfigureSource(document.get(), kTestSource1, kSettingStatusInvalid, {});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource0,
                           {{Key(kTestSource1), Key(kTestSource2)}}));
  CheckSentinelValues({}, {kTestSource1, kTestSource2});
}

TEST_F(SettingsDocumentManagerTest, TrustChangeDeletion) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);
  CheckSentinelValues({kTestSource1}, {});

  // Remove trust via a deletion. The sentinel value should disappear.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  document->SetDeletion(Key(MakeSourceKey(kTestSource1)));
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusSuccess,
      InsertDocument(std::move(document), kTestSource0, {{Key(kTestSource1)}}));
  CheckSentinelValues({}, {kTestSource1});
}

TEST_F(SettingsDocumentManagerTest, TrustChangeAccessRules) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);
  CheckSentinelValues({kTestSource1}, {});

  // Revoke source1's ability to write its sentinel.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  document->SetKey(
      MakeSourceKey(kTestSource1)
          .Extend({keys::sources::kAccess, kTestSource1}),
      MakeStringValue(SettingStatusToString(kSettingStatusInvalid)));
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusSuccess,
      InsertDocument(std::move(document), kTestSource0, {{Key(kTestSource1)}}));
  CheckSentinelValues({}, {kTestSource1});
}

TEST_F(SettingsDocumentManagerTest, TrustChangeWithdrawnSource) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);
  CheckSentinelValues({kTestSource1}, {});

  // Switch the source to withdrawn state.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  ConfigureSource(document.get(), kTestSource1, kSettingStatusWithdrawn, {});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource0, {{}}));

  // The value should still remain present.
  CheckSentinelValues({kTestSource1}, {});

  // source1 may no longer change the value.
  document = MakeDocument(kTestSource1);
  document->SetKey(Key(kTestSource1), MakeStringValue("change"));
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusAccessViolation,
            InsertDocument(std::move(document), kTestSource1, {}));
}

TEST_F(SettingsDocumentManagerTest, TrustChangeWithdrawnAccessRules) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);
  CheckSentinelValues({kTestSource1}, {});

  // Change access rule for the sentinel key to withdrawn.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  ConfigureSource(document.get(), kTestSource1, kSettingStatusActive,
                  {{Key(kTestSource1), kSettingStatusWithdrawn}});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource0, {{}}));

  // The value should still remain present.
  CheckSentinelValues({kTestSource1}, {});

  // source1 may no longer change the value.
  document = MakeDocument(kTestSource1);
  document->SetKey(Key(kTestSource1), MakeStringValue("change"));
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusAccessViolation,
            InsertDocument(std::move(document), kTestSource1, {}));
}

TEST_F(SettingsDocumentManagerTest, InsertionFailureStatus) {
  // Configure source1, but in invalid state.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  ConfigureSource(document.get(), kTestSource1, kSettingStatusInvalid,
                  {{Key(kTestSource1), kSettingStatusActive}});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource0, {{}}));

  // Inserting a document should fail.
  document = MakeDocument(kTestSource1);
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusAccessViolation,
            InsertDocument(std::move(document), kTestSource1, {}));
}

TEST_F(SettingsDocumentManagerTest, InsertionFailureAccessRules) {
  ConfigureTrustedSource(kTestSource1);

  // Inserting a document with a key the source can't write to should fail.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource1));
  document->SetKey(Key("A"), MakeIntValue(42));
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusAccessViolation,
            InsertDocument(std::move(document), kTestSource1, {}));
}

TEST_F(SettingsDocumentManagerTest, InsertionFailureVersionClash) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);

  // Inserting a document with an already-used version stamp component for the
  // issuing source should fail, even if there is no collision.
  VersionStamp version_stamp = current_version_;
  version_stamp.Set(kTestSource2, current_version_.Get(kTestSource2) + 1);
  EXPECT_TRUE(version_stamp.IsAfter(current_version_));
  std::unique_ptr<MockSettingsDocument> document(
      new MockSettingsDocument(version_stamp));
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusVersionClash,
            InsertDocument(std::move(document), kTestSource1, {}));
}

TEST_F(SettingsDocumentManagerTest, InsertionFailureVersionCollision) {
  ConfigureTrustedSource(kTestSource1);
  ConfigureTrustedSource(kTestSource2);

  VersionStamp initial_version = current_version_;

  // Insert a value for |kSharedKey|.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource1));
  document->SetKey(Key(kSharedKey), MakeIntValue(42));
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource1, {{}}));

  // Construct a colliding document, which should fail insertion.
  VersionStamp previous_version(current_version_);
  current_version_ = initial_version;
  document = MakeDocument(kTestSource2);
  document->SetKey(Key(kSharedKey), MakeIntValue(0));
  EXPECT_TRUE(previous_version.IsConcurrent(current_version_));
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusCollision,
            InsertDocument(std::move(document), kTestSource2, {}));
}

}  // namespace settingsd