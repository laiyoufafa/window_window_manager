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

#include "window_layout_policy_cascade.h"
#include "window_helper.h"
#include "window_inner_manager.h"
#include "window_manager_hilog.h"
#include "wm_trace.h"

namespace OHOS {
namespace Rosen {
namespace {
    constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HILOG_DOMAIN_WINDOW, "WindowLayoutPolicyCascade"};
    constexpr uint32_t WINDOW_CASCADE_HEIGHT = 48;
    constexpr uint32_t WINDOW_CASCADE_WIDTH = 48;
}
WindowLayoutPolicyCascade::WindowLayoutPolicyCascade(const Rect& displayRect, const uint64_t& screenId,
    const sptr<WindowNode>& belowAppNode, const sptr<WindowNode>& appNode, const sptr<WindowNode>& aboveAppNode)
    : WindowLayoutPolicy(displayRect, screenId, belowAppNode, appNode, aboveAppNode)
{
}

void WindowLayoutPolicyCascade::Launch()
{
    UpdateDisplayInfo();
    WLOGFI("WindowLayoutPolicyCascade::Launch");
}

void WindowLayoutPolicyCascade::Clean()
{
    WLOGFI("WindowLayoutPolicyCascade::Clean");
}

void WindowLayoutPolicyCascade::UpdateDisplayInfo()
{
    InitSplitRects();
    InitLimitRects();
    // UpdateDefaultFoatingRect();
    InitCascadeRect();
}

void WindowLayoutPolicyCascade::LayoutWindowNode(sptr<WindowNode>& node)
{
    if (node == nullptr) {
        return;
    }
    if (node->parent_ != nullptr) { // isn't root node
        if (!node->currentVisibility_) {
            WLOGFI("window[%{public}d] currently not visible, no need layout", node->GetWindowId());
            return;
        }
        UpdateLayoutRect(node);
        if (avoidTypes_.find(node->GetWindowType()) != avoidTypes_.end()) {
            UpdateLimitRect(node, limitRect_);
            UpdateSplitLimitRect(limitRect_, primaryLimitRect_);
            UpdateSplitLimitRect(limitRect_, secondaryLimitRect_);
            WLOGFI("priLimitRect: %{public}d %{public}d %{public}d %{public}d, " \
                "secLimitRect: %{public}d %{public}d %{public}d %{public}d", primaryLimitRect_.posX_,
                primaryLimitRect_.posY_, primaryLimitRect_.width_, primaryLimitRect_.height_, secondaryLimitRect_.posX_,
                secondaryLimitRect_.posY_, secondaryLimitRect_.width_, secondaryLimitRect_.height_);
        }
    }
    for (auto& childNode : node->children_) {
        LayoutWindowNode(childNode);
    }
}

void WindowLayoutPolicyCascade::LayoutWindowTree()
{
    InitLimitRects();
    std::vector<sptr<WindowNode>> rootNodes = { aboveAppWindowNode_, appWindowNode_, belowAppWindowNode_ };
    for (auto& node : rootNodes) { // ensure that the avoid area windows are traversed first
        LayoutWindowNode(node);
    }
}

void WindowLayoutPolicyCascade::UpdateWindowNode(sptr<WindowNode>& node)
{
    WM_FUNCTION_TRACE();
    auto type = node->GetWindowType();
    // affect other windows, trigger off global layout
    if (avoidTypes_.find(type) != avoidTypes_.end()) {
        LayoutWindowTree();
    } else if (type == WindowType::WINDOW_TYPE_DOCK_SLICE) { // split screen mode
        UpdateLayoutRect(node);
        auto splitDockerRect = node->GetLayoutRect();
        SetSplitRect(splitDockerRect); // calculate primary/secondary depend on divider rect
        WLOGFI("UpdateDividerRects WinId: %{public}d, Rect: %{public}d %{public}d %{public}d %{public}d",
            node->GetWindowId(), splitDockerRect.posX_, splitDockerRect.posY_,
            splitDockerRect.width_, splitDockerRect.height_);
        LayoutWindowTree();
    } else { // layout single window
        LayoutWindowNode(node);
    }
}

void WindowLayoutPolicyCascade::AddWindowNode(sptr<WindowNode>& node)
{
    WM_FUNCTION_TRACE();
    if (WindowHelper::IsEmptyRect(node->GetWindowProperty()->GetWindowRect())) {
        SetCascadeRect(node);
    }
    if (node->GetWindowType() == WindowType::WINDOW_TYPE_DOCK_SLICE) {
        node->SetWindowRect(dividerRect_); // init divider bar
    }
    UpdateWindowNode(node); // currently, update and add do the same process
}

static bool IsLayoutChanged(const Rect& l, const Rect& r)
{
    return !((l.posX_ == r.posX_) && (l.posY_ == r.posY_) && (l.width_ == r.width_) && (l.height_ == r.height_));
}

void WindowLayoutPolicyCascade::LimitMoveBounds(Rect& rect)
{
    Rect curRect = rect;
    rect.posX_ = std::max(curRect.posX_, limitRect_.posX_);
    rect.posY_ = std::max(curRect.posY_, limitRect_.posY_);
    if (rect.width_ < rect.height_) {
        rect.posX_ = std::min(curRect.posX_ + rect.width_,
                              limitRect_.posX_ + limitRect_.width_) - rect.width_;
        rect.height_ = curRect.posY_ + curRect.height_ - rect.posY_;
    } else {
        rect.posY_ = std::min(curRect.posY_ + rect.height_,
                              limitRect_.posY_ + limitRect_.height_) - rect.height_;
        rect.width_ = curRect.posX_ + curRect.width_ - rect.posX_;
    }
}

void WindowLayoutPolicyCascade::InitCascadeRect()
{
    constexpr uint32_t half = 2;
    constexpr float ratio = 0.35;  // 0.75: default height/width ratio

    // calculate default H and w
    uint32_t defaultW = static_cast<uint32_t>(displayRect_.width_ * ratio);
    uint32_t defaultH = static_cast<uint32_t>(displayRect_.height_ * ratio);

    // calculate default x and y
    Rect resRect = {0, 0, defaultW, defaultH};
    if (defaultW <= limitRect_.width_ && defaultH <= limitRect_.height_) {
        int32_t centerPosX = limitRect_.posX_ + static_cast<int32_t>(limitRect_.width_ / half);
        resRect.posX_ = centerPosX - static_cast<int32_t>(defaultW / half);

        int32_t centerPosY = limitRect_.posY_ + static_cast<int32_t>(limitRect_.height_ / half);
        resRect.posY_ = centerPosY - static_cast<int32_t>(defaultH / half);
    }
    firstCascadeRect = resRect;
    curCascadeRect = firstCascadeRect;
}

void WindowLayoutPolicyCascade::UpdateLayoutRect(sptr<WindowNode>& node)
{
    auto type = node->GetWindowType();
    auto mode = node->GetWindowMode();
    auto flags = node->GetWindowFlags();
    auto decorEnbale = node->GetWindowProperty()->GetDecorEnable();
    bool needAvoid = (flags & static_cast<uint32_t>(WindowFlag::WINDOW_FLAG_NEED_AVOID));
    bool parentLimit = (flags & static_cast<uint32_t>(WindowFlag::WINDOW_FLAG_PARENT_LIMIT));
    bool subWindow = WindowHelper::IsSubWindow(type);
    bool floatingWindow = (mode == WindowMode::WINDOW_MODE_FLOATING);
    const Rect& layoutRect = node->GetLayoutRect();
    Rect lastRect = layoutRect;
    Rect displayRect = GetDisplayRect(mode);
    Rect limitRect = displayRect;
    Rect winRect = node->GetWindowProperty()->GetWindowRect();

    WLOGFI("Id:%{public}d, avoid:%{public}d parLimit:%{public}d floating:%{public}d, sub:%{public}d, " \
        "deco:%{public}d, type:%{public}d, requestRect:[%{public}d, %{public}d, %{public}d, %{public}d]",
        node->GetWindowId(), needAvoid, parentLimit, floatingWindow, subWindow, decorEnbale,
        static_cast<uint32_t>(type), winRect.posX_, winRect.posY_, winRect.width_, winRect.height_);
    if (needAvoid) {
        limitRect = GetLimitRect(mode);
    }

    if (!floatingWindow) { // fullscreen window
        winRect = limitRect;
    } else { // floating window
        if (node->GetWindowProperty()->GetDecorEnable()) { // is decorable
            winRect = ComputeDecoratedWindowRect(winRect);
        }
        if (subWindow && parentLimit) { // subwidow and limited by parent
            limitRect = node->parent_->GetLayoutRect();
            UpdateFloatingLayoutRect(limitRect, winRect);
        }
    }
    // Limit window to the maximum window size
    LimitWindowSize(node, displayRect, winRect);

    if (node->GetWindowType() == WindowType::WINDOW_TYPE_DOCK_SLICE) {
        LimitMoveBounds(winRect);
    }

    node->SetLayoutRect(winRect);
    if (IsLayoutChanged(lastRect, winRect)) {
        node->GetWindowToken()->UpdateWindowRect(winRect);
        node->surfaceNode_->SetBounds(winRect.posX_, winRect.posY_, winRect.width_, winRect.height_);
    }
}

void WindowLayoutPolicyCascade::InitLimitRects()
{
    limitRect_ = displayRect_;
    primaryLimitRect_ = primaryRect_;
    secondaryLimitRect_ = secondaryRect_;
}

Rect WindowLayoutPolicyCascade::GetLimitRect(const WindowMode mode) const
{
    if (mode == WindowMode::WINDOW_MODE_SPLIT_PRIMARY) {
        return primaryLimitRect_;
    } else if (mode == WindowMode::WINDOW_MODE_SPLIT_SECONDARY) {
        return secondaryLimitRect_;
    } else {
        return limitRect_;
    }
}

Rect WindowLayoutPolicyCascade::GetDisplayRect(const WindowMode mode) const
{
    if (mode == WindowMode::WINDOW_MODE_SPLIT_PRIMARY) {
        return primaryRect_;
    } else if (mode == WindowMode::WINDOW_MODE_SPLIT_SECONDARY) {
        return secondaryRect_;
    } else {
        return displayRect_;
    }
}

void WindowLayoutPolicyCascade::UpdateSplitLimitRect(const Rect& limitRect, Rect& limitSplitRect)
{
    Rect curLimitRect = limitSplitRect;
    limitSplitRect.posX_ = std::max(limitRect.posX_, curLimitRect.posX_);
    limitSplitRect.posY_ = std::max(limitRect.posY_, curLimitRect.posY_);
    limitSplitRect.width_ = std::min(limitRect.posX_ + limitRect.width_,
                                     curLimitRect.posX_ + curLimitRect.width_) -
                                     limitSplitRect.posX_;
    limitSplitRect.height_ = std::min(limitRect.posY_ + limitRect.height_,
                                      curLimitRect.posY_ + curLimitRect.height_) -
                                      limitSplitRect.posY_;
}

void WindowLayoutPolicyCascade::InitSplitRects()
{
    if (!IsVertical()) {
        dividerRect_ = { static_cast<uint32_t>((displayRect_.width_ - DIVIDER_WIDTH) * DEFAULT_SPLIT_RATIO), 0,
                DIVIDER_WIDTH, displayRect_.height_ };
    } else {
        dividerRect_ = { 0, static_cast<uint32_t>((displayRect_.height_ - DIVIDER_WIDTH) * DEFAULT_SPLIT_RATIO),
               displayRect_.width_, DIVIDER_WIDTH };
    }
    WLOGFI("init dividerRect :[%{public}d, %{public}d, %{public}d, %{public}d]",
        dividerRect_.posX_, dividerRect_.posY_, dividerRect_.width_, dividerRect_.height_);
    SetSplitRect(dividerRect_);
}

bool WindowLayoutPolicyCascade::IsVertical() const
{
    return displayRect_.width_ < displayRect_.height_;
}

void WindowLayoutPolicyCascade::SetSplitRectByRatio(float ratio)
{
    if (!IsVertical()) {
        dividerRect_.posX_ = limitRect_.posX_ +
            static_cast<uint32_t>((limitRect_.width_ - dividerRect_.width_) * ratio);
    } else {
        dividerRect_.posY_ = limitRect_.posY_ +
            static_cast<uint32_t>((limitRect_.height_ - dividerRect_.height_) * ratio);
    }
    WLOGFI("set dividerRect :[%{public}d, %{public}d, %{public}d, %{public}d]",
        dividerRect_.posX_, dividerRect_.posY_, dividerRect_.width_, dividerRect_.height_);
    SetSplitRect(dividerRect_);
}

void WindowLayoutPolicyCascade::SetSplitRect(const Rect& divRect)
{
    dividerRect_.width_ = divRect.width_;
    dividerRect_.height_ = divRect.height_;
    if (!IsVertical()) {
        primaryRect_.posX_ = displayRect_.posX_;
        primaryRect_.posY_ = displayRect_.posY_;
        primaryRect_.width_ = divRect.posX_;
        primaryRect_.height_ = displayRect_.height_;

        secondaryRect_.posX_ = divRect.posX_ + dividerRect_.width_;
        secondaryRect_.posY_ = displayRect_.posY_;
        secondaryRect_.width_ = displayRect_.width_ - secondaryRect_.posX_;
        secondaryRect_.height_ = displayRect_.height_;
    } else {
        primaryRect_.posX_ = displayRect_.posX_;
        primaryRect_.posY_ = displayRect_.posY_;
        primaryRect_.height_ = divRect.posY_;
        primaryRect_.width_ = displayRect_.width_;

        secondaryRect_.posX_ = displayRect_.posX_;
        secondaryRect_.posY_ = divRect.posY_ + dividerRect_.height_;
        secondaryRect_.height_ = displayRect_.height_ - secondaryRect_.posY_;
        secondaryRect_.width_ = displayRect_.width_;
    }
}

void WindowLayoutPolicyCascade::ReOrderWindowNode(const sptr<WindowNode>& node)
{
    Rect rect = { 0, 0, 0, 0 };
    bool flag = true;
    for (auto iter = appWindowNode_->children_.begin(); iter != appWindowNode_->children_.end(); iter++) {
        if ((*iter)->GetWindowType() == WindowType::WINDOW_TYPE_APP_MAIN_WINDOW) {
            if (flag) {
                rect = firstCascadeRect;
                flag = false;
            } else {
                rect.width_ = rect.width_;
                rect.height_ = rect.height_;
                rect.posX_ = (rect.posX_ + WINDOW_CASCADE_WIDTH >= limitRect_.posX_) &&
                             (rect.posX_ + rect.width_ + WINDOW_CASCADE_WIDTH <= displayRect_.width_) ?
                             (rect.posX_ + WINDOW_CASCADE_WIDTH) : limitRect_.posX_;
                rect.posY_ = (rect.posY_ + WINDOW_CASCADE_HEIGHT >= limitRect_.posY_) &&
                             (rect.posY_ + rect.height_ + WINDOW_CASCADE_HEIGHT <= displayRect_.height_) ?
                             (rect.posY_ + WINDOW_CASCADE_HEIGHT) : limitRect_.posY_;
                curCascadeRect = rect;
            }
            node->SetWindowRect(rect);
            return;
        }
    }
}

Rect WindowLayoutPolicyCascade::GetCurCascadeRect(const sptr<WindowNode>& node)
{
    Rect cascadeRect = {0, 0, 0, 0};
    for (auto iter = appWindowNode_->children_.rbegin(); iter != appWindowNode_->children_.rend(); iter++) {
        WLOGFI("GetCurCascadeRect id:[%{public}d,", (*iter)->GetWindowId());
        if ((*iter)->GetWindowType() != WindowType::WINDOW_TYPE_DOCK_SLICE &&
            (*iter)->GetWindowId() != node->GetWindowId()) {
            cascadeRect = (*iter)->GetWindowProperty()->GetWindowRect();
            WLOGFI("get current cascadeRect :[%{public}d, %{public}d, %{public}d, %{public}d]",
                    cascadeRect.posX_, cascadeRect.posY_, cascadeRect.width_, cascadeRect.height_);
            break;
        }
    }
    if (WindowHelper::IsEmptyRect(cascadeRect)) {
        WLOGFI("cascade rect is empty use first cascade rect");
        return firstCascadeRect;
    }
    return StepCascadeRect(cascadeRect);
}

Rect WindowLayoutPolicyCascade::StepCascadeRect(Rect rect)
{
    Rect cascadeRect = {0, 0, 0, 0};
    cascadeRect.width_ = rect.width_;
    cascadeRect.height_ = rect.height_;
    cascadeRect.posX_ = (rect.posX_ + WINDOW_CASCADE_WIDTH >= limitRect_.posX_) &&
                    (rect.posX_ + rect.width_ + WINDOW_CASCADE_WIDTH <= displayRect_.width_) ?
                    (rect.posX_ + WINDOW_CASCADE_WIDTH) : limitRect_.posX_;
    cascadeRect.posY_ = (rect.posY_ + WINDOW_CASCADE_HEIGHT >= limitRect_.posY_) &&
                    (rect.posY_ + rect.height_ + WINDOW_CASCADE_HEIGHT <= displayRect_.height_) ?
                    (rect.posY_ + WINDOW_CASCADE_HEIGHT) : limitRect_.posY_;
    WLOGFI("step cascadeRect :[%{public}d, %{public}d, %{public}d, %{public}d]",
        cascadeRect.posX_, cascadeRect.posY_, cascadeRect.width_, cascadeRect.height_);
    return cascadeRect;
}

void WindowLayoutPolicyCascade::SetCascadeRect(const sptr<WindowNode>& node)
{
    static bool isFirstAppWindow = true;
    Rect rect;
    if (WindowHelper::IsAppWindow(node->GetWindowProperty()->GetWindowType()) && isFirstAppWindow) {
        WLOGFI("set first app window cascade rect");
        rect = firstCascadeRect;
        isFirstAppWindow = false;
    } else if (WindowHelper::IsAppWindow(node->GetWindowProperty()->GetWindowType()) && !isFirstAppWindow) {
        WLOGFI("set other app window cascade rect");
        curCascadeRect = GetCurCascadeRect(node);
        rect = curCascadeRect;
    } else {
        // system window
        WLOGFI("set system window cascade rect");
        rect = firstCascadeRect;
    }
    WLOGFI("set  cascadeRect :[%{public}d, %{public}d, %{public}d, %{public}d]",
        rect.posX_, rect.posY_, rect.width_, rect.height_);
    node->SetWindowRect(rect);
}
}
}
