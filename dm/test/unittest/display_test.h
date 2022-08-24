/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FRAMEWORKS_DM_TEST_UT_DISPLAY_TEST_H
#define FRAMEWORKS_DM_TEST_UT_DISPLAY_TEST_H

#include <gtest/gtest.h>
#include "display.h"
#include "display_manager.h"

namespace OHOS {
namespace Rosen {
class DisplayTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    virtual void SetUp() override;
    virtual void TearDown() override;

    static sptr<Display> defaultDisplay_;
    static DisplayId defaultDisplayId_;
};
} // namespace Rosen
} // namespace OHOS

#endif // FRAMEWORKS_DM_TEST_UT_DISPLAY_TEST_H