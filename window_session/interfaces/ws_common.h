/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef OHOS_ROSEN_WINDOW_SESSION_COMMON_H
#define OHOS_ROSEN_WINDOW_SESSION_COMMON_H

#include <string>
#include <map>
namespace OHOS::Rosen {
namespace {
    constexpr uint32_t INVALID_SESSION_ID = 0;
}

enum class WSError : int32_t {
    WS_OK = 0,
    WS_DO_NOTHING,
    WS_ERROR_IPC_FAILED,
    WS_ERROR_NULLPTR,
    WS_ERROR_INVALID_SESSION,
};

enum class SessionState : uint32_t {
    STATE_DISCONNECT = 0,
    STATE_CONNECT,
    STATE_FOREGROUND,
    STATE_ACTIVE,
    STATE_INACTIVE,
    STATE_BACKGROUND,
};

enum class SizeChangeReason : uint32_t {
    SHOW = 0,
    HIDE,
    MAXIMIZE,
    RECOVER,
    ROTATION,
};

struct WSRect {
    int32_t posX_;
    int32_t posY_;
    uint32_t width_;
    uint32_t height_;

    bool operator==(const WSRect& a) const
    {
        return (posX_ == a.posX_ && posY_ == a.posY_ && width_ == a.width_ && height_ == a.height_);
    }

    bool operator!=(const WSRect& a) const
    {
        return !this->operator==(a);
    }

    bool isUninitializedRect() const
    {
        return (posX_ == 0 && posY_ == 0 && width_ == 0 && height_ == 0);
    }

    bool IsInsideOf(const WSRect& a) const
    {
        return (posX_ >= a.posX_ && posY_ >= a.posY_ &&
            posX_ + width_ <= a.posX_ + a.width_ && posY_ + height_ <= a.posY_ + a.height_);
    }
};
}
#endif // OHOS_ROSEN_WINDOW_SESSION_COMMON_H
