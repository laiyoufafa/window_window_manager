/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "window_node_container.h"

#include <ability_manager_client.h>
#include <algorithm>
#include <cinttypes>
#include <ctime>
#include <display_power_mgr_client.h>
#include <power_mgr_client.h>

#include "common_event_manager.h"
#include "display_manager_service_inner.h"
#include "dm_common.h"
#include "window_helper.h"
#include "window_inner_manager.h"
#include "window_layout_policy_cascade.h"
#include "window_layout_policy_tile.h"
#include "window_manager_agent_controller.h"
#include "window_manager_hilog.h"
#include "wm_common.h"
#include "wm_common_inner.h"
#include "wm_trace.h"

namespace OHOS {
namespace Rosen {
namespace {
    constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HILOG_DOMAIN_WINDOW, "WindowNodeContainer"};
    constexpr int WINDOW_NAME_MAX_LENGTH = 10;
    const std::string SPLIT_SCREEN_EVENT_NAME = "common.event.SPLIT_SCREEN";
    const char DISABLE_WINDOW_ANIMATION_PATH[] = "/etc/disable_window_animation";
    constexpr uint32_t MAX_BRIGHTNESS = 255;
}

WindowNodeContainer::WindowNodeContainer(DisplayId displayId, uint32_t width, uint32_t height)
{
    Rect displayRect = {
        .posX_ = 0,
        .posY_ = 0,
        .width_ = width,
        .height_ = height
    };
    displayRectMap_.insert(std::make_pair(displayId, displayRect));
    windowPair_ = new WindowPair(displayId, appWindowNode_);

    // init window node maps
    InitWindowNodeMapForDisplay(displayId);

    // init layout policy
    layoutPolicys_[WindowLayoutMode::CASCADE] = new WindowLayoutPolicyCascade(displayRectMap_, windowNodeMaps_);
    layoutPolicys_[WindowLayoutMode::TILE] = new WindowLayoutPolicyTile(displayRectMap_, windowNodeMaps_);
    layoutPolicy_ = layoutPolicys_[WindowLayoutMode::CASCADE];
    layoutPolicy_->Launch();

    // init avoidAreaController
    UpdateAvoidAreaFunc func = std::bind(&WindowNodeContainer::OnAvoidAreaChange, this,
        std::placeholders::_1, std::placeholders::_2);
    avoidController_ = new AvoidAreaController(displayId, func);

    // init systembar map
    InitSysBarMapForDisplay(displayId);
}

WindowNodeContainer::~WindowNodeContainer()
{
    Destroy();
}

void WindowNodeContainer::InitSysBarMapForDisplay(DisplayId displayId)
{
    SysBarNodeMap sysBarNodeMap {
        { WindowType::WINDOW_TYPE_STATUS_BAR,     nullptr },
        { WindowType::WINDOW_TYPE_NAVIGATION_BAR, nullptr },
    };
    sysBarNodeMaps_.insert(std::make_pair(displayId, sysBarNodeMap));

    SysBarTintMap sysBarTintMap {
        { WindowType::WINDOW_TYPE_STATUS_BAR,     SystemBarRegionTint() },
        { WindowType::WINDOW_TYPE_NAVIGATION_BAR, SystemBarRegionTint() },
    };
    sysBarTintMaps_.insert(std::make_pair(displayId, sysBarTintMap));
}

void WindowNodeContainer::InitWindowNodeMapForDisplay(DisplayId displayId)
{
    std::map<WindowRootNodeType, std::unique_ptr<std::vector<sptr<WindowNode>>>> windowRootNodeMap;
    windowRootNodeMap.insert(std::make_pair(WindowRootNodeType::APP_WINDOW_NODE,
        std::make_unique<std::vector<sptr<WindowNode>>>()));
    windowRootNodeMap.insert(std::make_pair(WindowRootNodeType::ABOVE_WINDOW_NODE,
        std::make_unique<std::vector<sptr<WindowNode>>>()));
    windowRootNodeMap.insert(std::make_pair(WindowRootNodeType::BELOW_WINDOW_NODE,
        std::make_unique<std::vector<sptr<WindowNode>>>()));
    windowNodeMaps_.insert(std::make_pair(displayId, std::move(windowRootNodeMap)));
}

int WindowNodeContainer::GetWindowCountByType(WindowType windowType)
{
    int windowNumber = 0;
    auto counter = [&windowNumber, &windowType](sptr<WindowNode>& windowNode) {
        if (windowNode->GetWindowType() == windowType) ++windowNumber;
    };
    std::for_each(belowAppWindowNode_->children_.begin(), belowAppWindowNode_->children_.end(), counter);
    std::for_each(appWindowNode_->children_.begin(), appWindowNode_->children_.end(), counter);
    std::for_each(aboveAppWindowNode_->children_.begin(), aboveAppWindowNode_->children_.end(), counter);
    return windowNumber;
}

WMError WindowNodeContainer::MinimizeStructuredAppWindowsExceptSelf(const sptr<WindowNode>& node)
{
    std::vector<uint32_t> exceptionalIds = { node->GetWindowId() };
    std::vector<WindowMode> exceptionalModes = { WindowMode::WINDOW_MODE_FLOATING, WindowMode::WINDOW_MODE_PIP };
    return MinimizeAppNodeExceptOptions(false, exceptionalIds, exceptionalModes);
}

std::vector<sptr<WindowNode>>* WindowNodeContainer::FindNodeVectorOfRoot(DisplayId displayId, WindowRootNodeType type)
{
    if (windowNodeMaps_.find(displayId) != windowNodeMaps_.end()) {
        auto& rootNodemap = windowNodeMaps_[displayId];
        if (rootNodemap.find(type) != rootNodemap.end()) {
            return rootNodemap[type].get();
        }
    }
    return nullptr;
}

void WindowNodeContainer::AddWindowNodeInRootNodeVector(sptr<WindowNode>& node, WindowRootNodeType rootType)
{
    std::vector<sptr<WindowNode>>* rootNodeVectorPtr = FindNodeVectorOfRoot(node->GetDisplayId(), rootType);
    if (rootNodeVectorPtr != nullptr) {
        rootNodeVectorPtr->push_back(node);
        WLOGFI("add node in node vector of root, windowId: %{public}d, rootType: %{public}d",
            node->GetWindowId(), rootType);
    } else {
        WLOGFE("add node failed, rootNode vector is empty, windowId: %{public}d, rootType: %{public}d",
            node->GetWindowId(), rootType);
    }
}

void WindowNodeContainer::RemoveWindowNodeFromRootNodeVector(sptr<WindowNode>& node, WindowRootNodeType rootType)
{
    std::vector<sptr<WindowNode>>* rootNodeVectorPtr = FindNodeVectorOfRoot(node->GetDisplayId(), rootType);
    if (rootNodeVectorPtr != nullptr) {
        auto iter = std::remove(rootNodeVectorPtr->begin(), rootNodeVectorPtr->end(), node);
        rootNodeVectorPtr->erase(iter, rootNodeVectorPtr->end());
        WLOGFI("remove node from node vector of root, windowId: %{public}d, rootType: %{public}d",
            node->GetWindowId(), rootType);
    } else {
        WLOGFE("remove node failed, rootNode vector is empty, windowId: %{public}d, rootType: %{public}d",
            node->GetWindowId(), rootType);
    }
}

void WindowNodeContainer::UpdateWindowNodeMaps()
{
    for (auto& elem : windowNodeMaps_) {
        for (auto& nodeVec : elem.second) {
            auto emptyVector = std::vector<sptr<WindowNode>>();
            nodeVec.second->swap(emptyVector);
        }
    }

    std::vector<sptr<WindowNode>> rootNodes = { aboveAppWindowNode_, appWindowNode_, belowAppWindowNode_ };
    std::vector<WindowRootNodeType> rootNodeType = {
        WindowRootNodeType::ABOVE_WINDOW_NODE,
        WindowRootNodeType::APP_WINDOW_NODE,
        WindowRootNodeType::BELOW_WINDOW_NODE
    };
    for (size_t index = 0; index < rootNodes.size(); ++index) {
        auto rootType = rootNodeType[index];
        for (auto& node : rootNodes[index]->children_) {
            AddWindowNodeInRootNodeVector(node, rootType);
        }
    }
}

WMError WindowNodeContainer::AddWindowNodeOnWindowTree(sptr<WindowNode>& node, sptr<WindowNode>& parentNode)
{
    sptr<WindowNode> root = FindRoot(node->GetWindowType());
    if (root == nullptr) {
        WLOGFE("root is nullptr!");
        return WMError::WM_ERROR_NULLPTR;
    }
    node->requestedVisibility_ = true;
    if (parentNode != nullptr) { // subwindow
        if (parentNode->parent_ != root &&
            !((parentNode->GetWindowFlags() & static_cast<uint32_t>(WindowFlag::WINDOW_FLAG_SHOW_WHEN_LOCKED)) &&
            (parentNode->parent_ == aboveAppWindowNode_))) {
            WLOGFE("window type and parent window not match or try to add subwindow to subwindow, which is forbidden");
            return WMError::WM_ERROR_INVALID_PARAM;
        }
        node->currentVisibility_ = parentNode->currentVisibility_;
        node->parent_ = parentNode;
    } else { // mainwindow
        node->parent_ = root;
        node->currentVisibility_ = true;
        for (auto& child : node->children_) {
            child->currentVisibility_ = child->requestedVisibility_;
        }
        if (WindowHelper::IsAvoidAreaWindow(node->GetWindowType())) {
            sysBarNodeMaps_[node->GetDisplayId()][node->GetWindowType()] = node;
        }
    }
    return WMError::WM_OK;
}

WMError WindowNodeContainer::ShowInTransition(sptr<WindowNode>& node)
{
    return WMError::WM_OK;
}

WMError WindowNodeContainer::AddWindowNode(sptr<WindowNode>& node, sptr<WindowNode>& parentNode)
{
    if (!node->surfaceNode_) {
        WLOGFE("surface node is nullptr!");
        return WMError::WM_ERROR_NULLPTR;
    }

    WMError ret = AddWindowNodeOnWindowTree(node, parentNode);
    if (ret != WMError::WM_OK) {
        WLOGFE("Add window failed!");
        return ret;
    }

    windowPair_->UpdateIfSplitRelated(node);

    UpdateWindowTree(node);
    if (node->IsSplitMode() || node->GetWindowType() == WindowType::WINDOW_TYPE_DOCK_SLICE) {
        RaiseSplitRelatedWindowToTop(node);
    }
    UpdateRSTree(node, true, node->isPlayAnimationShow_);
    AssignZOrder();
    layoutPolicy_->AddWindowNode(node);
    if (WindowHelper::IsAvoidAreaWindow(node->GetWindowType())) {
        avoidController_->AvoidControl(node, AvoidControlType::AVOID_NODE_ADD);
        NotifyIfSystemBarRegionChanged(node->GetDisplayId());
    } else {
        NotifyIfSystemBarTintChanged(node->GetDisplayId());
    }
    std::vector<sptr<WindowVisibilityInfo>> infos;
    UpdateWindowVisibilityInfos(infos);
    DumpScreenWindowTree();
    NotifyAccessibilityWindowInfo(node, WindowUpdateType::WINDOW_UPDATE_ADDED);
    WLOGFI("AddWindowNode windowId: %{public}u end", node->GetWindowId());
    return WMError::WM_OK;
}

WMError WindowNodeContainer::UpdateWindowNode(sptr<WindowNode>& node, WindowUpdateReason reason)
{
    if (WindowHelper::IsMainWindow(node->GetWindowType()) && WindowHelper::IsSwitchCascadeReason(reason)) {
        SwitchLayoutPolicy(WindowLayoutMode::CASCADE, node->GetDisplayId());
    }
    layoutPolicy_->UpdateWindowNode(node);
    if (WindowHelper::IsAvoidAreaWindow(node->GetWindowType())) {
        avoidController_->AvoidControl(node, AvoidControlType::AVOID_NODE_UPDATE);
        NotifyIfSystemBarRegionChanged(node->GetDisplayId());
    } else {
        NotifyIfSystemBarTintChanged(node->GetDisplayId());
    }
    DumpScreenWindowTree();
    WLOGFI("UpdateWindowNode windowId: %{public}u end", node->GetWindowId());
    return WMError::WM_OK;
}

void WindowNodeContainer::UpdateSizeChangeReason(sptr<WindowNode>& node, WindowSizeChangeReason reason)
{
    if (!node->GetWindowToken()) {
        WLOGFE("windowToken is null");
        return;
    }
    if (node->GetWindowType() == WindowType::WINDOW_TYPE_DOCK_SLICE) {
        for (auto& childNode : appWindowNode_->children_) {
            if (childNode->IsSplitMode()) {
                childNode->GetWindowToken()->UpdateWindowRect(childNode->GetWindowRect(),
                    childNode->GetDecoStatus(), reason);
                childNode->ResetWindowSizeChangeReason();
                WLOGFI("Notify split window that the drag action is start or end, windowId: %{public}d, "
                    "reason: %{public}u", childNode->GetWindowId(), reason);
            }
        }
    } else {
        node->GetWindowToken()->UpdateWindowRect(node->GetWindowRect(), node->GetDecoStatus(), reason);
        node->ResetWindowSizeChangeReason();
        WLOGFI("Notify window that the drag action is start or end, windowId: %{public}d, "
            "reason: %{public}u", node->GetWindowId(), reason);
    }
}

void WindowNodeContainer::UpdateWindowTree(sptr<WindowNode>& node)
{
    WM_FUNCTION_TRACE();
    node->priority_ = zorderPolicy_->GetWindowPriority(node->GetWindowType());
    RaiseInputMethodWindowPriorityIfNeeded(node);
    RaiseShowWhenLockedWindowIfNeeded(node);
    auto parentNode = node->parent_;
    auto position = parentNode->children_.end();
    for (auto iter = parentNode->children_.begin(); iter < parentNode->children_.end(); ++iter) {
        if ((*iter)->priority_ > node->priority_) {
            position = iter;
            break;
        }
    }
    parentNode->children_.insert(position, node);
}

bool WindowNodeContainer::UpdateRSTree(sptr<WindowNode>& node, bool isAdd, bool animationPlayed)
{
    WM_FUNCTION_TRACE();
    static const bool IsWindowAnimationEnabled = ReadIsWindowAnimationEnabledProperty();
    DisplayId displayId = node->GetDisplayId();
    auto updateRSTreeFunc = [&]() {
        auto& dms = DisplayManagerServiceInner::GetInstance();
        if (isAdd) {
            dms.UpdateRSTree(displayId, node->surfaceNode_, true);
            for (auto& child : node->children_) {
                if (child->currentVisibility_) {
                    dms.UpdateRSTree(displayId, child->surfaceNode_, true);
                }
            }
        } else {
            if (node->leashWinSurfaceNode_) {
                dms.UpdateRSTree(displayId, node->leashWinSurfaceNode_, false);
            } else {
                dms.UpdateRSTree(displayId, node->surfaceNode_, false);
            }
            for (auto& child : node->children_) {
                dms.UpdateRSTree(displayId, child->surfaceNode_, false);
            }
        }
    };

    if (IsWindowAnimationEnabled && !animationPlayed) {
        // default transition duration: 350ms
        static const RSAnimationTimingProtocol timingProtocol(350);
        // default transition curve: EASE OUT
        static const Rosen::RSAnimationTimingCurve curve = Rosen::RSAnimationTimingCurve::EASE_OUT;
        // add or remove window with transition animation
        RSNode::Animate(timingProtocol, curve, updateRSTreeFunc);
    } else {
        // add or remove window without animation
        updateRSTreeFunc();
    }
    return true;
}

WMError WindowNodeContainer::DestroyWindowNode(sptr<WindowNode>& node, std::vector<uint32_t>& windowIds)
{
    WMError ret = RemoveWindowNode(node);
    if (ret != WMError::WM_OK) {
        WLOGFE("RemoveWindowNode failed");
        return ret;
    }
    node->leashWinSurfaceNode_ = nullptr;
    node->startingWinSurfaceNode_ = nullptr;
    node->surfaceNode_ = nullptr;
    windowIds.push_back(node->GetWindowId());

    for (auto& child : node->children_) { // destroy sub window if exists
        windowIds.push_back(child->GetWindowId());
        child->parent_ = nullptr;
        if (child->surfaceNode_ != nullptr) {
            WLOGFI("child surfaceNode set nullptr");
            child->surfaceNode_ = nullptr;
        }
    }
    auto emptyVector = std::vector<sptr<WindowNode>>();
    node->children_.swap(emptyVector);
    return WMError::WM_OK;
}

void WindowNodeContainer::RemoveWindowNodeFromWindowTree(sptr<WindowNode>& node)
{
    // remove this node from node vector of display
    sptr<WindowNode> root = FindRoot(node->GetWindowType());

    // remove this node from parent
    auto iter = std::find(node->parent_->children_.begin(), node->parent_->children_.end(), node);
    if (iter != node->parent_->children_.end()) {
        node->parent_->children_.erase(iter);
    } else {
        WLOGFE("can't find this node in parent");
    }
    node->parent_ = nullptr;
}

WMError WindowNodeContainer::RemoveWindowNode(sptr<WindowNode>& node)
{
    if (node == nullptr) {
        WLOGFE("window node or surface node is nullptr, invalid");
        return WMError::WM_ERROR_DESTROYED_OBJECT;
    }

    if (node->parent_ == nullptr) {
        WLOGFW("can't find parent of this node");
    } else {
        RemoveWindowNodeFromWindowTree(node);
    }

    node->requestedVisibility_ = false;
    node->currentVisibility_ = false;
    node->isCovered_ = true;
    std::vector<sptr<WindowVisibilityInfo>> infos = {new WindowVisibilityInfo(node->GetWindowId(),
        node->GetCallingPid(), node->GetCallingUid(), false)};
    for (auto& child : node->children_) {
        if (child->currentVisibility_) {
            child->currentVisibility_ = false;
            child->isCovered_ = true;
            infos.emplace_back(new WindowVisibilityInfo(child->GetWindowId(), child->GetCallingPid(),
                child->GetCallingUid(), false));
        }
    }
    UpdateRSTree(node, false, node->isPlayAnimationHide_);
    UpdateWindowNodeMaps();
    layoutPolicy_->RemoveWindowNode(node);
    windowPair_->HandleRemoveWindow(node);
    if (WindowHelper::IsAvoidAreaWindow(node->GetWindowType())) {
        avoidController_->AvoidControl(node, AvoidControlType::AVOID_NODE_REMOVE);
        NotifyIfSystemBarRegionChanged(node->GetDisplayId());
    } else {
        NotifyIfSystemBarTintChanged(node->GetDisplayId());
    }
    UpdateWindowVisibilityInfos(infos);
    DumpScreenWindowTree();
    NotifyAccessibilityWindowInfo(node, WindowUpdateType::WINDOW_UPDATE_REMOVED);
    RcoveryScreenDefaultOrientationIfNeed(node->GetDisplayId());
    WLOGFI("RemoveWindowNode windowId: %{public}u end", node->GetWindowId());
    return WMError::WM_OK;
}

void WindowNodeContainer::RcoveryScreenDefaultOrientationIfNeed(DisplayId displayId)
{
    if (windowNodeMaps_[displayId][WindowRootNodeType::APP_WINDOW_NODE]->empty()) {
        WLOGFI("appWindowNode_ child is empty in display  %{public}" PRIu64"", displayId);
        DisplayManagerServiceInner::GetInstance().
            SetOrientationFromWindow(displayId, Orientation::UNSPECIFIED);
    }
}

const std::vector<uint32_t>& WindowNodeContainer::Destroy()
{
    auto emptyVector = std::vector<uint32_t>();
    removedIds_.swap(emptyVector);
    for (auto& node : belowAppWindowNode_->children_) {
        DestroyWindowNode(node, removedIds_);
    }
    for (auto& node : appWindowNode_->children_) {
        DestroyWindowNode(node, removedIds_);
    }
    for (auto& node : aboveAppWindowNode_->children_) {
        DestroyWindowNode(node, removedIds_);
    }
    return removedIds_;
}

sptr<WindowNode> WindowNodeContainer::FindRoot(WindowType type) const
{
    if (WindowHelper::IsAppWindow(type) || type == WindowType::WINDOW_TYPE_DOCK_SLICE) {
        return appWindowNode_;
    }
    if (WindowHelper::IsBelowSystemWindow(type)) {
        return belowAppWindowNode_;
    }
    if (WindowHelper::IsAboveSystemWindow(type)) {
        return aboveAppWindowNode_;
    }
    return nullptr;
}

sptr<WindowNode> WindowNodeContainer::FindWindowNodeById(uint32_t id) const
{
    std::vector<sptr<WindowNode>> rootNodes = { aboveAppWindowNode_, appWindowNode_, belowAppWindowNode_ };
    for (auto& rootNode : rootNodes) {
        for (auto& node : rootNode->children_) {
            if (node->GetWindowId() == id) {
                return node;
            }
            for (auto& subNode : node->children_) {
                if (subNode->GetWindowId() == id) {
                    return subNode;
                }
            }
        }
    }
    return nullptr;
}

void WindowNodeContainer::UpdateFocusStatus(uint32_t id, bool focused) const
{
    auto node = FindWindowNodeById(id);
    if (node == nullptr) {
        WLOGFW("cannot find focused window id:%{public}d", id);
    } else {
        if (node->GetWindowToken()) {
            node->GetWindowToken()->UpdateFocusStatus(focused);
        }
        if (node->abilityToken_ == nullptr) {
            WLOGFI("abilityToken is null, window : %{public}d", id);
        }
        if (focused) {
            WLOGFW("current focus window: windowId: %{public}d, windowName: %{public}s",
                id, node->GetWindowProperty()->GetWindowName().c_str());
        }
        sptr<FocusChangeInfo> focusChangeInfo = new FocusChangeInfo(node->GetWindowId(), node->GetDisplayId(),
            node->GetCallingPid(), node->GetCallingUid(), node->GetWindowType(), node->abilityToken_);
        WindowManagerAgentController::GetInstance().UpdateFocusChangeInfo(
            focusChangeInfo, focused);
    }
}

void WindowNodeContainer::UpdateActiveStatus(uint32_t id, bool isActive) const
{
    auto node = FindWindowNodeById(id);
    if (node == nullptr) {
        WLOGFE("cannot find active window id: %{public}d", id);
        return;
    }
    if (node->GetWindowToken()) {
        node->GetWindowToken()->UpdateActiveStatus(isActive);
    }
}

void WindowNodeContainer::UpdateBrightness(uint32_t id, bool byRemoved)
{
    auto node = FindWindowNodeById(id);
    if (node == nullptr) {
        WLOGFE("cannot find active window id: %{public}d", id);
        return;
    }

    if (!byRemoved) {
        if (!WindowHelper::IsAppWindow(node->GetWindowType())) {
            return;
        }
    }
    WLOGFI("brightness: [%{public}f, %{public}f]", GetDisplayBrightness(), node->GetBrightness());
    if (node->GetBrightness() == UNDEFINED_BRIGHTNESS) {
        if (GetDisplayBrightness() != node->GetBrightness()) {
            WLOGFI("adjust brightness with default value");
            DisplayPowerMgr::DisplayPowerMgrClient::GetInstance().RestoreBrightness();
            SetDisplayBrightness(UNDEFINED_BRIGHTNESS); // UNDEFINED_BRIGHTNESS means system default brightness
        }
        SetBrightnessWindow(INVALID_WINDOW_ID);
    } else {
        if (GetDisplayBrightness() != node->GetBrightness()) {
            WLOGFI("adjust brightness with value: %{public}u", ToOverrideBrightness(node->GetBrightness()));
            DisplayPowerMgr::DisplayPowerMgrClient::GetInstance().OverrideBrightness(
                ToOverrideBrightness(node->GetBrightness()));
            SetDisplayBrightness(node->GetBrightness());
        }
        SetBrightnessWindow(node->GetWindowId());
    }
}

void WindowNodeContainer::AssignZOrder()
{
    zOrder_ = 0;
    WindowNodeOperationFunc func = [this](sptr<WindowNode> node) {
        if (node->surfaceNode_ == nullptr) {
            WLOGE("AssignZOrder: surfaceNode is nullptr, window Id:%{public}u", node->GetWindowId());
            return false;
        }
        node->surfaceNode_->SetPositionZ(zOrder_);
        ++zOrder_;
        return false;
    };
    TraverseWindowTree(func, false);
    UpdateWindowNodeMaps();
}

WMError WindowNodeContainer::SetFocusWindow(uint32_t windowId)
{
    if (focusedWindow_ == windowId) {
        WLOGFI("focused window do not change, id: %{public}u", windowId);
        return WMError::WM_DO_NOTHING;
    }
    UpdateFocusStatus(focusedWindow_, false);
    focusedWindow_ = windowId;
    sptr<WindowNode> node = FindWindowNodeById(windowId);
    NotifyAccessibilityWindowInfo(node, WindowUpdateType::WINDOW_UPDATE_FOCUSED);
    UpdateFocusStatus(focusedWindow_, true);
    return WMError::WM_OK;
}

uint32_t WindowNodeContainer::GetFocusWindow() const
{
    return focusedWindow_;
}

WMError WindowNodeContainer::SetActiveWindow(uint32_t windowId, bool byRemoved)
{
    if (activeWindow_ == windowId) {
        WLOGFI("active window do not change, id: %{public}u", windowId);
        return WMError::WM_DO_NOTHING;
    }
    UpdateActiveStatus(activeWindow_, false);
    activeWindow_ = windowId;
    UpdateActiveStatus(activeWindow_, true);
    UpdateBrightness(activeWindow_, byRemoved);
    return WMError::WM_OK;
}

void WindowNodeContainer::SetDisplayBrightness(float brightness)
{
    displayBrightness_ = brightness;
}

float WindowNodeContainer::GetDisplayBrightness() const
{
    return displayBrightness_;
}

void WindowNodeContainer::SetBrightnessWindow(uint32_t windowId)
{
    brightnessWindow_ = windowId;
}

uint32_t WindowNodeContainer::GetBrightnessWindow() const
{
    return brightnessWindow_;
}

uint32_t WindowNodeContainer::ToOverrideBrightness(float brightness)
{
    return static_cast<uint32_t>(brightness * MAX_BRIGHTNESS);
}

uint32_t WindowNodeContainer::GetActiveWindow() const
{
    return activeWindow_;
}

void WindowNodeContainer::HandleKeepScreenOn(const sptr<WindowNode>& node, bool requireLock)
{
    if (requireLock && node->keepScreenLock_ == nullptr) {
        // reset ipc identity
        std::string identity = IPCSkeleton::ResetCallingIdentity();
        node->keepScreenLock_ = PowerMgr::PowerMgrClient::GetInstance().CreateRunningLock(node->GetWindowName(),
            PowerMgr::RunningLockType::RUNNINGLOCK_SCREEN);
        // set ipc identity to raw
        IPCSkeleton::SetCallingIdentity(identity);
    }
    if (node->keepScreenLock_ == nullptr) {
        return;
    }
    WLOGFI("handle keep screen on: [%{public}s, %{public}d]", node->GetWindowName().c_str(), requireLock);
    ErrCode res;
    // reset ipc identity
    std::string identity = IPCSkeleton::ResetCallingIdentity();
    if (requireLock) {
        res = node->keepScreenLock_->Lock();
    } else {
        res = node->keepScreenLock_->UnLock();
    }
    // set ipc identity to raw
    IPCSkeleton::SetCallingIdentity(identity);
    if (res != ERR_OK) {
        WLOGFE("handle keep screen running lock failed: [operation: %{public}d, err: %{public}d]", requireLock, res);
    }
}

bool WindowNodeContainer::IsAboveSystemBarNode(sptr<WindowNode> node) const
{
    int32_t curPriority = zorderPolicy_->GetWindowPriority(node->GetWindowType());
    if ((curPriority > zorderPolicy_->GetWindowPriority(WindowType::WINDOW_TYPE_STATUS_BAR)) &&
        (curPriority > zorderPolicy_->GetWindowPriority(WindowType::WINDOW_TYPE_NAVIGATION_BAR))) {
        return true;
    }
    return false;
}

bool WindowNodeContainer::IsFullImmersiveNode(sptr<WindowNode> node) const
{
    auto mode = node->GetWindowMode();
    auto flags = node->GetWindowFlags();
    return mode == WindowMode::WINDOW_MODE_FULLSCREEN &&
        !(flags & static_cast<uint32_t>(WindowFlag::WINDOW_FLAG_NEED_AVOID));
}

bool WindowNodeContainer::IsSplitImmersiveNode(sptr<WindowNode> node) const
{
    auto type = node->GetWindowType();
    return node->IsSplitMode() || type == WindowType::WINDOW_TYPE_DOCK_SLICE;
}

std::unordered_map<WindowType, SystemBarProperty> WindowNodeContainer::GetExpectImmersiveProperty() const
{
    std::unordered_map<WindowType, SystemBarProperty> sysBarPropMap {
        { WindowType::WINDOW_TYPE_STATUS_BAR,     SystemBarProperty() },
        { WindowType::WINDOW_TYPE_NAVIGATION_BAR, SystemBarProperty() },
    };

    std::vector<sptr<WindowNode>> rootNodes = { aboveAppWindowNode_, appWindowNode_, belowAppWindowNode_ };
    for (auto& node : rootNodes) {
        for (auto iter = node->children_.rbegin(); iter < node->children_.rend(); ++iter) {
            auto& sysBarPropMapNode = (*iter)->GetSystemBarProperty();
            if (IsAboveSystemBarNode(*iter)) {
                continue;
            }
            if (IsFullImmersiveNode(*iter)) {
                WLOGFI("Top immersive window id: %{public}d. Use full immersive prop", (*iter)->GetWindowId());
                for (auto it : sysBarPropMap) {
                    sysBarPropMap[it.first] = (sysBarPropMapNode.find(it.first))->second;
                }
                return sysBarPropMap;
            } else if (IsSplitImmersiveNode(*iter)) {
                WLOGFI("Top split window id: %{public}d. Use split immersive prop", (*iter)->GetWindowId());
                for (auto it : sysBarPropMap) {
                    sysBarPropMap[it.first] = (sysBarPropMapNode.find(it.first))->second;
                    sysBarPropMap[it.first].enable_ = false;
                }
                return sysBarPropMap;
            }
        }
    }

    WLOGFI("No immersive window on top. Use default systembar Property");
    return sysBarPropMap;
}

void WindowNodeContainer::NotifyIfSystemBarTintChanged(DisplayId displayId)
{
    WM_FUNCTION_TRACE();
    auto expectSystemBarProp = GetExpectImmersiveProperty();
    SystemBarRegionTints tints;
    SysBarTintMap& sysBarTintMap = sysBarTintMaps_[displayId];
    for (auto it : sysBarTintMap) {
        auto expectProp = expectSystemBarProp.find(it.first)->second;
        if (it.second.prop_ == expectProp) {
            continue;
        }
        WLOGFI("System bar prop update, Type: %{public}d, Visible: %{public}d, Color: %{public}x | %{public}x",
            static_cast<int32_t>(it.first), expectProp.enable_, expectProp.backgroundColor_, expectProp.contentColor_);
        sysBarTintMap[it.first].prop_ = expectProp;
        sysBarTintMap[it.first].type_ = it.first;
        tints.emplace_back(sysBarTintMap[it.first]);
    }
    WindowManagerAgentController::GetInstance().UpdateSystemBarRegionTints(displayId, tints);
}

void WindowNodeContainer::NotifyIfSystemBarRegionChanged(DisplayId displayId)
{
    WM_FUNCTION_TRACE();
    SystemBarRegionTints tints;
    SysBarTintMap& sysBarTintMap = sysBarTintMaps_[displayId];
    SysBarNodeMap& sysBarNodeMap = sysBarNodeMaps_[displayId];
    for (auto it : sysBarTintMap) { // split screen mode not support yet
        auto sysNode = sysBarNodeMap[it.first];
        if (sysNode == nullptr || it.second.region_ == sysNode->GetWindowRect()) {
            continue;
        }
        const Rect& newRegion = sysNode->GetWindowRect();
        sysBarTintMap[it.first].region_ = newRegion;
        sysBarTintMap[it.first].type_ = it.first;
        tints.emplace_back(sysBarTintMap[it.first]);
        WLOGFI("system bar region update, type: %{public}d" \
            "region: [%{public}d, %{public}d, %{public}d, %{public}d]",
            static_cast<int32_t>(it.first), newRegion.posX_, newRegion.posY_, newRegion.width_, newRegion.height_);
    }
    WindowManagerAgentController::GetInstance().UpdateSystemBarRegionTints(displayId, tints);
}

void WindowNodeContainer::NotifySystemBarDismiss(sptr<WindowNode>& node)
{
    WM_FUNCTION_TRACE();
    if (node == nullptr) {
        WLOGE("could not find window");
        return;
    }
    SystemBarRegionTints tints;
    auto& sysBarPropMapNode = node->GetSystemBarProperty();
    SysBarTintMap& sysBarTintMap = sysBarTintMaps_[node->GetDisplayId()];
    for (auto it : sysBarPropMapNode) {
        it.second.enable_ = false;
        node->SetSystemBarProperty(it.first, it.second);
        WLOGFI("set system bar enable to false, id: %{public}u, type: %{public}d",
            node->GetWindowId(), static_cast<int32_t>(it.first));
        if (sysBarTintMap[it.first].prop_.enable_) {
            sysBarTintMap[it.first].prop_.enable_ = false;
            tints.emplace_back(sysBarTintMap[it.first]);
            WLOGFI("notify system bar dismiss, type: %{public}d", static_cast<int32_t>(it.first));
        }
    }
    WindowManagerAgentController::GetInstance().UpdateSystemBarRegionTints(node->GetDisplayId(), tints);
}

void WindowNodeContainer::NotifySystemBarTints(std::vector<DisplayId> displayIdVec)
{
    WM_FUNCTION_TRACE();
    if (displayIdVec.size() != sysBarTintMaps_.size()) {
        WLOGE("the number of display is error");
    }

    for (auto displayId : displayIdVec) {
        SystemBarRegionTints tints;
        SysBarTintMap& sysBarTintMap = sysBarTintMaps_[displayId];
        for (auto it : sysBarTintMap) {
            WLOGFI("system bar cur notify, type: %{public}d, " \
                "visible: %{public}d, color: %{public}x | %{public}x, " \
                "region: [%{public}d, %{public}d, %{public}d, %{public}d]",
                static_cast<int32_t>(it.first),
                sysBarTintMap[it.first].prop_.enable_,
                sysBarTintMap[it.first].prop_.backgroundColor_, sysBarTintMap[it.first].prop_.contentColor_,
                sysBarTintMap[it.first].region_.posX_, sysBarTintMap[it.first].region_.posY_,
                sysBarTintMap[it.first].region_.width_, sysBarTintMap[it.first].region_.height_);
            tints.push_back(sysBarTintMap[it.first]);
        }
        WindowManagerAgentController::GetInstance().UpdateSystemBarRegionTints(displayId, tints);
    }
}

bool WindowNodeContainer::IsTopWindow(uint32_t windowId, sptr<WindowNode>& rootNode) const
{
    if (rootNode->children_.empty()) {
        WLOGFE("root does not have any node");
        return false;
    }
    auto node = *(rootNode->children_.rbegin());
    if (node == nullptr) {
        WLOGFE("window tree does not have any node");
        return false;
    }

    for (auto iter = node->children_.rbegin(); iter < node->children_.rend(); iter++) {
        if ((*iter)->priority_ > 0) {
            return (*iter)->GetWindowId() == windowId;
        } else {
            break;
        }
    }
    return node->GetWindowId() == windowId;
}

void WindowNodeContainer::RaiseOrderedWindowToTop(std::vector<sptr<WindowNode>>& orderedNodes,
    std::vector<sptr<WindowNode>>& windowNodes)
{
    for (auto iter = appWindowNode_->children_.begin(); iter != appWindowNode_->children_.end();) {
        uint32_t wid = (*iter)->GetWindowId();
        auto orderedIter = std::find_if(orderedNodes.begin(), orderedNodes.end(),
            [wid] (sptr<WindowNode> orderedNode) { return orderedNode->GetWindowId() == wid; });
        if (orderedIter != orderedNodes.end()) {
            iter = windowNodes.erase(iter);
        } else {
            iter++;
        }
    }
    for (auto iter = orderedNodes.begin(); iter != orderedNodes.end(); iter++) {
        UpdateWindowTree(*iter);
    }
    return;
}

void WindowNodeContainer::RaiseWindowToTop(uint32_t windowId, std::vector<sptr<WindowNode>>& windowNodes)
{
    auto iter = std::find_if(windowNodes.begin(), windowNodes.end(),
                             [windowId](sptr<WindowNode> node) {
                                 return node->GetWindowId() == windowId;
                             });
    // raise app node window to top
    if (iter != windowNodes.end()) {
        sptr<WindowNode> node = *iter;
        windowNodes.erase(iter);
        UpdateWindowTree(node);
        WLOGFI("raise window to top %{public}u", node->GetWindowId());
    }
}

void WindowNodeContainer::NotifyAccessibilityWindowInfo(const sptr<WindowNode>& node, WindowUpdateType type) const
{
    if (node == nullptr) {
        WLOGFE("window node is null");
        return;
    }
    bool isNeedNotify = false;
    switch (type) {
        case WindowUpdateType::WINDOW_UPDATE_ADDED:
            if (node->currentVisibility_) {
                isNeedNotify = true;
            }
            break;
        case WindowUpdateType::WINDOW_UPDATE_FOCUSED:
            if (node->GetWindowId() == focusedWindow_) {
                isNeedNotify = true;
            }
            break;
        case WindowUpdateType::WINDOW_UPDATE_REMOVED:
            isNeedNotify = true;
            break;
        default:
            break;
    }
    if (isNeedNotify) {
        std::vector<sptr<WindowInfo>> windowList;
        GetWindowList(windowList);
        sptr<WindowInfo> windowInfo = new (std::nothrow) WindowInfo();
        sptr<AccessibilityWindowInfo> accessibilityWindowInfo = new (std::nothrow) AccessibilityWindowInfo();
        if (windowInfo != nullptr && accessibilityWindowInfo != nullptr) {
            windowInfo->wid_ = static_cast<int32_t>(node->GetWindowId());
            windowInfo->windowRect_ = node->GetWindowRect();
            windowInfo->focused_ = node->GetWindowId() == focusedWindow_;
            windowInfo->displayId_ = node->GetDisplayId();
            windowInfo->mode_ = node->GetWindowMode();
            windowInfo->type_ = node->GetWindowType();
            accessibilityWindowInfo->currentWindowInfo_ = windowInfo;
            accessibilityWindowInfo->windowList_ = windowList;
            WindowManagerAgentController::GetInstance().NotifyAccessibilityWindowInfo(accessibilityWindowInfo, type);
        }
    }
}

void WindowNodeContainer::GetWindowList(std::vector<sptr<WindowInfo>>& windowList) const
{
    std::vector<sptr<WindowNode>> windowNodes;
    TraverseContainer(windowNodes);
    for (auto node : windowNodes) {
        sptr<WindowInfo> windowInfo = new WindowInfo();
        windowInfo->wid_ = static_cast<int32_t>(node->GetWindowId());
        windowInfo->windowRect_ = node->GetWindowRect();
        windowInfo->focused_ = node->GetWindowId() == focusedWindow_;
        windowInfo->displayId_ = node->GetDisplayId();
        windowInfo->mode_ = node->GetWindowMode();
        windowInfo->type_ = node->GetWindowType();
        windowList.emplace_back(windowInfo);
    }
}

void WindowNodeContainer::TraverseContainer(std::vector<sptr<WindowNode>>& windowNodes) const
{
    for (auto& node : belowAppWindowNode_->children_) {
        TraverseWindowNode(node, windowNodes);
    }
    for (auto& node : appWindowNode_->children_) {
        TraverseWindowNode(node, windowNodes);
    }
    for (auto& node : aboveAppWindowNode_->children_) {
        TraverseWindowNode(node, windowNodes);
    }
    std::reverse(windowNodes.begin(), windowNodes.end());
}

void WindowNodeContainer::TraverseWindowNode(sptr<WindowNode>& node, std::vector<sptr<WindowNode>>& windowNodes) const
{
    if (node == nullptr) {
        return;
    }
    auto iter = node->children_.begin();
    for (; iter < node->children_.end(); ++iter) {
        if ((*iter)->priority_ < 0) {
            windowNodes.emplace_back(*iter);
        } else {
            break;
        }
    }
    windowNodes.emplace_back(node);
    for (; iter < node->children_.end(); ++iter) {
        windowNodes.emplace_back(*iter);
    }
}

std::vector<Rect> WindowNodeContainer::GetAvoidAreaByType(AvoidAreaType avoidAreaType, DisplayId displayId)
{
    return avoidController_->GetAvoidAreaByType(avoidAreaType, displayId);
}

void WindowNodeContainer::OnAvoidAreaChange(const std::vector<Rect>& avoidArea, DisplayId displayId)
{
    for (auto& node : appWindowNode_->children_) {
        if (node->GetWindowMode() == WindowMode::WINDOW_MODE_FULLSCREEN && node->GetWindowToken() != nullptr) {
            // notify client
            node->GetWindowToken()->UpdateAvoidArea(avoidArea);
        }
    }
}

void WindowNodeContainer::DumpScreenWindowTree()
{
    WLOGFI("-------- dump window info begin---------");
    WLOGFI("WindowName DisplayId WinId Type Mode Flag ZOrd Orientation [   x    y    w    h]");
    uint32_t zOrder = zOrder_;
    WindowNodeOperationFunc func = [&zOrder](sptr<WindowNode> node) {
        Rect rect = node->GetWindowRect();
        const std::string& windowName = node->GetWindowName().size() < WINDOW_NAME_MAX_LENGTH ?
            node->GetWindowName() : node->GetWindowName().substr(0, WINDOW_NAME_MAX_LENGTH);
        WLOGI("DumpScreenWindowTree: %{public}10s %{public}9" PRIu64" %{public}5u %{public}4u %{public}4u %{public}4u "
            "%{public}4u %{public}11u [%{public}4d %{public}4d %{public}4u %{public}4u]",
            windowName.c_str(), node->GetDisplayId(), node->GetWindowId(), node->GetWindowType(), node->GetWindowMode(),
            node->GetWindowFlags(), --zOrder, static_cast<uint32_t>(node->GetRequestedOrientation()),
            rect.posX_, rect.posY_, rect.width_, rect.height_);
        return false;
    };
    TraverseWindowTree(func, true);
    WLOGFI("-------- dump window info end  ---------");
}

uint64_t WindowNodeContainer::GetScreenId(DisplayId displayId) const
{
    return DisplayManagerServiceInner::GetInstance().GetRSScreenId(displayId);
}

Rect WindowNodeContainer::GetDisplayRect(DisplayId displayId) const
{
    return const_cast<WindowNodeContainer*>(this)->displayRectMap_[displayId];
}

bool WindowNodeContainer::isVerticalDisplay(DisplayId displayId) const
{
    return const_cast<WindowNodeContainer*>(this)->displayRectMap_[displayId].width_ <
        const_cast<WindowNodeContainer*>(this)->displayRectMap_[displayId].height_;
}

void WindowNodeContainer::ProcessWindowStateChange(WindowState state, WindowStateChangeReason reason)
{
    switch (reason) {
        case WindowStateChangeReason::KEYGUARD: {
            int32_t topPriority = zorderPolicy_->GetWindowPriority(WindowType::WINDOW_TYPE_KEYGUARD);
            TraverseAndUpdateWindowState(state, topPriority);
            break;
        }
        default:
            return;
    }
}

void WindowNodeContainer::TraverseAndUpdateWindowState(WindowState state, int32_t topPriority)
{
    std::vector<sptr<WindowNode>> rootNodes = { belowAppWindowNode_, appWindowNode_, aboveAppWindowNode_ };
    for (auto& node : rootNodes) {
        UpdateWindowState(node, topPriority, state);
    }
}

void WindowNodeContainer::UpdateWindowState(sptr<WindowNode> node, int32_t topPriority, WindowState state)
{
    if (node == nullptr) {
        return;
    }
    if (node->parent_ != nullptr && node->currentVisibility_) {
        if (node->priority_ < topPriority &&
            !(node->GetWindowFlags() & static_cast<uint32_t>(WindowFlag::WINDOW_FLAG_SHOW_WHEN_LOCKED))) {
            if (node->GetWindowToken()) {
                node->GetWindowToken()->UpdateWindowState(state);
            }
            HandleKeepScreenOn(node, state);
        }
    }
    for (auto& childNode : node->children_) {
        UpdateWindowState(childNode, topPriority, state);
    }
}

void WindowNodeContainer::HandleKeepScreenOn(const sptr<WindowNode>& node, WindowState state)
{
    if (node == nullptr) {
        WLOGFE("window is invalid");
        return;
    }
    if (state == WindowState::STATE_FROZEN) {
        HandleKeepScreenOn(node, false);
    } else if (state == WindowState::STATE_UNFROZEN) {
        HandleKeepScreenOn(node, node->IsKeepScreenOn());
    } else {
        // do nothing
    }
}

sptr<WindowNode> WindowNodeContainer::FindDividerNode() const
{
    for (auto iter = appWindowNode_->children_.begin(); iter != appWindowNode_->children_.end(); iter++) {
        if ((*iter)->GetWindowType() == WindowType::WINDOW_TYPE_DOCK_SLICE) {
            return *iter;
        }
    }
    return nullptr;
}

void WindowNodeContainer::RaiseSplitRelatedWindowToTop(sptr<WindowNode>& node)
{
    if (node == nullptr) {
        return;
    }
    std::vector<sptr<WindowNode>> orderedPair = windowPair_->GetOrderedPair(node);
    RaiseOrderedWindowToTop(orderedPair, appWindowNode_->children_);
    AssignZOrder();
    return;
}

WMError WindowNodeContainer::RaiseZOrderForAppWindow(sptr<WindowNode>& node, sptr<WindowNode>& parentNode)
{
    if (node == nullptr) {
        return WMError::WM_ERROR_NULLPTR;
    }

    if (IsTopWindow(node->GetWindowId(), appWindowNode_) || IsTopWindow(node->GetWindowId(), aboveAppWindowNode_)) {
        WLOGFI("it is already top app window, id: %{public}u", node->GetWindowId());
        return WMError::WM_ERROR_INVALID_TYPE;
    }

    if (WindowHelper::IsSubWindow(node->GetWindowType())) {
        if (parentNode == nullptr) {
            WLOGFE("window type is invalid");
            return WMError::WM_ERROR_NULLPTR;
        }
        RaiseWindowToTop(node->GetWindowId(), parentNode->children_); // raise itself
        if (parentNode->IsSplitMode()) {
            RaiseSplitRelatedWindowToTop(parentNode);
        } else {
            RaiseWindowToTop(node->GetParentId(), parentNode->parent_->children_); // raise parent window
        }
    } else if (WindowHelper::IsMainWindow(node->GetWindowType())) {
        if (node->IsSplitMode()) {
            RaiseSplitRelatedWindowToTop(node);
        } else {
            RaiseWindowToTop(node->GetWindowId(), node->parent_->children_);
        }
    } else {
        // do nothing
    }
    AssignZOrder();
    WLOGFI("RaiseZOrderForAppWindow finished");
    DumpScreenWindowTree();
    return WMError::WM_OK;
}

sptr<WindowNode> WindowNodeContainer::GetNextFocusableWindow(uint32_t windowId) const
{
    sptr<WindowNode> nextFocusableWindow;
    bool previousFocusedWindowFound = false;
    WindowNodeOperationFunc func = [windowId, &nextFocusableWindow, &previousFocusedWindowFound](
        sptr<WindowNode> node) {
        if (previousFocusedWindowFound && node->GetWindowProperty()->GetFocusable()) {
            nextFocusableWindow = node;
            return true;
        }
        if (node->GetWindowId() == windowId) {
            previousFocusedWindowFound = true;
        }
        return false;
    };
    TraverseWindowTree(func, true);
    return nextFocusableWindow;
}

sptr<WindowNode> WindowNodeContainer::GetNextActiveWindow(uint32_t windowId) const
{
    auto currentNode = FindWindowNodeById(windowId);
    if (currentNode == nullptr) {
        WLOGFE("cannot find window id: %{public}u by tree", windowId);
        return nullptr;
    }
    WLOGFI("current window: [%{public}u, %{public}u]", windowId, static_cast<uint32_t>(currentNode->GetWindowType()));
    if (WindowHelper::IsSystemWindow(currentNode->GetWindowType())) {
        for (auto& node : appWindowNode_->children_) {
            if (node->GetWindowType() == WindowType::WINDOW_TYPE_DOCK_SLICE) {
                continue;
            }
            return node;
        }
        for (auto& node : belowAppWindowNode_->children_) {
            if (node->GetWindowType() == WindowType::WINDOW_TYPE_DESKTOP) {
                return node;
            }
        }
    } else if (WindowHelper::IsAppWindow(currentNode->GetWindowType())) {
        std::vector<sptr<WindowNode>> windowNodes;
        TraverseContainer(windowNodes);
        auto iter = std::find_if(windowNodes.begin(), windowNodes.end(), [windowId](sptr<WindowNode>& node) {
            return node->GetWindowId() == windowId;
            });
        if (iter == windowNodes.end()) {
            WLOGFE("could not find this window");
            return nullptr;
        }
        int index = std::distance(windowNodes.begin(), iter);
        for (size_t i = static_cast<size_t>(index) + 1; i < windowNodes.size(); i++) {
            if (windowNodes[i]->GetWindowType() == WindowType::WINDOW_TYPE_DOCK_SLICE) {
                continue;
            }
            return windowNodes[i];
        }
    } else {
        // do nothing
    }
    WLOGFE("could not get next active window");
    return nullptr;
}

void WindowNodeContainer::MinimizeAllAppWindows(DisplayId displayId)
{
    WMError ret =  MinimizeAppNodeExceptOptions(true);
    SwitchLayoutPolicy(WindowLayoutMode::CASCADE, displayId);
    if (ret != WMError::WM_OK) {
        WLOGFE("Minimize all app window failed");
    }
    return;
}

void WindowNodeContainer::MinimizeOldestAppWindow()
{
    for (auto& appNode : appWindowNode_->children_) {
        if (appNode->GetWindowType() == WindowType::WINDOW_TYPE_APP_MAIN_WINDOW) {
            WLOGFI("minimize window, windowId:%{public}u", appNode->GetWindowId());
            MinimizeWindowFromAbility(appNode, true);
            return;
        }
    }
    for (auto& appNode : aboveAppWindowNode_->children_) {
        if (appNode->GetWindowType() == WindowType::WINDOW_TYPE_APP_MAIN_WINDOW) {
            WLOGFI("minimize window, windowId:%{public}u", appNode->GetWindowId());
            MinimizeWindowFromAbility(appNode, true);
            return;
        }
    }
    WLOGFI("no window needs to minimize");
}

void WindowNodeContainer::MinimizeWindowFromAbility(const sptr<WindowNode>& node, bool fromUser)
{
    if (node->abilityToken_ == nullptr) {
        WLOGFW("Target abilityToken is nullptr, windowId:%{public}u", node->GetWindowId());
        return;
    }
    WLOGFI("minimize window fromUser:%{public}d, isMinimizedByOther:%{public}d", fromUser, isMinimizedByOther_);
    if (!fromUser && !isMinimizedByOther_) {
        return;
    }
    AAFwk::AbilityManagerClient::GetInstance()->MinimizeAbility(node->abilityToken_, fromUser);
}

WMError WindowNodeContainer::MinimizeAppNodeExceptOptions(bool fromUser, const std::vector<uint32_t> &exceptionalIds,
                                                          const std::vector<WindowMode> &exceptionalModes)
{
    if (appWindowNode_->children_.empty()) {
        return WMError::WM_OK;
    }
    for (auto& appNode : appWindowNode_->children_) {
        // exclude exceptional window
        if (std::find(exceptionalIds.begin(), exceptionalIds.end(), appNode->GetWindowId()) != exceptionalIds.end() ||
            std::find(exceptionalModes.begin(), exceptionalModes.end(),
                appNode->GetWindowMode()) != exceptionalModes.end() ||
                appNode->GetWindowType() != WindowType::WINDOW_TYPE_APP_MAIN_WINDOW) {
            continue;
        }
        // minimize window
        WLOGFI("minimize window, windowId:%{public}u", appNode->GetWindowId());
        MinimizeWindowFromAbility(appNode, fromUser);
    }
    return WMError::WM_OK;
}

void WindowNodeContainer::ResetLayoutPolicy()
{
    layoutPolicy_->Reset();
}

WMError WindowNodeContainer::SwitchLayoutPolicy(WindowLayoutMode dstMode, DisplayId displayId, bool reorder)
{
    WLOGFI("SwitchLayoutPolicy src: %{public}d dst: %{public}d, reorder: %{public}d, displayId: %{public}" PRIu64"",
        static_cast<uint32_t>(layoutMode_), static_cast<uint32_t>(dstMode), static_cast<uint32_t>(reorder), displayId);
    if (dstMode < WindowLayoutMode::BASE || dstMode >= WindowLayoutMode::END) {
        WLOGFE("invalid layout mode");
        return WMError::WM_ERROR_INVALID_PARAM;
    }
    if (layoutMode_ != dstMode) {
        if (layoutMode_ == WindowLayoutMode::CASCADE) {
            layoutPolicy_->Reset();
            windowPair_->Clear();
        }
        layoutMode_ = dstMode;
        layoutPolicy_->Clean();
        layoutPolicy_ = layoutPolicys_[dstMode];
        layoutPolicy_->Launch();
        DumpScreenWindowTree();
    } else {
        WLOGFI("Current layout mode is already: %{public}d", static_cast<uint32_t>(dstMode));
    }
    if (reorder) {
        windowPair_->Clear();
        layoutPolicy_->Reorder();
        DumpScreenWindowTree();
    }
    NotifyIfSystemBarTintChanged(displayId);
    return WMError::WM_OK;
}

void WindowNodeContainer::RaiseInputMethodWindowPriorityIfNeeded(const sptr<WindowNode>& node) const
{
    if (node->GetWindowType() != WindowType::WINDOW_TYPE_INPUT_METHOD_FLOAT) {
        return;
    }
    auto iter = std::find_if(aboveAppWindowNode_->children_.begin(), aboveAppWindowNode_->children_.end(),
                             [](sptr<WindowNode> node) {
        return node->GetWindowType() == WindowType::WINDOW_TYPE_KEYGUARD;
    });
    if (iter != aboveAppWindowNode_->children_.end()) {
        WLOGFI("raise input method float window priority.");
        node->priority_ = zorderPolicy_->GetWindowPriority(WindowType::WINDOW_TYPE_KEYGUARD) + 1;
    }
}

void WindowNodeContainer::ReZOrderShowWhenLockedWindows(const sptr<WindowNode>& node, bool up)
{
    WLOGFI("Keyguard change %{public}u, re-zorder showWhenLocked window", up);
    std::vector<sptr<WindowNode>> needReZOrderNodes;
    auto& srcRoot = up ? appWindowNode_ : aboveAppWindowNode_;
    auto& dstRoot = up ? aboveAppWindowNode_ : appWindowNode_;

    auto dstPriority = up ? zorderPolicy_->GetWindowPriority(WindowType::WINDOW_TYPE_KEYGUARD) + 1 :
        zorderPolicy_->GetWindowPriority(WindowType::WINDOW_TYPE_APP_MAIN_WINDOW);

    for (auto iter = srcRoot->children_.begin(); iter != srcRoot->children_.end();) {
        if ((*iter)->GetWindowFlags() & static_cast<uint32_t>(WindowFlag::WINDOW_FLAG_SHOW_WHEN_LOCKED)) {
            needReZOrderNodes.emplace_back(*iter);
            iter = srcRoot->children_.erase(iter);
        } else {
            iter++;
        }
    }

    for (auto& needReZOrderNode : needReZOrderNodes) {
        needReZOrderNode->priority_ = dstPriority;
        needReZOrderNode->parent_ = dstRoot;
        auto parentNode = needReZOrderNode->parent_;
        auto position = parentNode->children_.end();
        for (auto iter = parentNode->children_.begin(); iter < parentNode->children_.end(); ++iter) {
            if ((*iter)->priority_ > needReZOrderNode->priority_) {
                position = iter;
                break;
            }
        }

        parentNode->children_.insert(position, needReZOrderNode);
        WLOGFI("ShowWhenLocked window %{public}u re-zorder when keyguard change %{public}u",
            needReZOrderNode->GetWindowId(), up);
    }
}

void WindowNodeContainer::RaiseShowWhenLockedWindowIfNeeded(const sptr<WindowNode>& node)
{
    // if keyguard window show, raise show when locked windows
    if (node->GetWindowType() == WindowType::WINDOW_TYPE_KEYGUARD) {
        ReZOrderShowWhenLockedWindows(node, true);
        return;
    }

    // if show when locked window show, raise itself when exist keyguard
    if (!(node->GetWindowFlags() & static_cast<uint32_t>(WindowFlag::WINDOW_FLAG_SHOW_WHEN_LOCKED))) {
        return;
    }

    auto iter = std::find_if(aboveAppWindowNode_->children_.begin(), aboveAppWindowNode_->children_.end(),
                             [](sptr<WindowNode> node) {
        return node->GetWindowType() == WindowType::WINDOW_TYPE_KEYGUARD;
    });
    if (iter != aboveAppWindowNode_->children_.end()) {
        WLOGFI("ShowWhenLocked window %{public}u raise itself", node->GetWindowId());
        node->priority_ = zorderPolicy_->GetWindowPriority(WindowType::WINDOW_TYPE_KEYGUARD) + 1;
        node->parent_ = aboveAppWindowNode_;
    }
}

void WindowNodeContainer::DropShowWhenLockedWindowIfNeeded(const sptr<WindowNode>& node)
{
    // if keyguard window hide, drop show when locked windows
    if (node->GetWindowType() == WindowType::WINDOW_TYPE_KEYGUARD) {
        ReZOrderShowWhenLockedWindows(node, false);
        AssignZOrder();
    }
}

void WindowNodeContainer::MoveWindowNodes(DisplayId displayId, std::vector<uint32_t>& windowIds)
{
    WLOGFI("Move window nodes when destroy display");
}

void WindowNodeContainer::ProcessDisplayCreate(DisplayId displayId, const Rect& displayRect)
{
    avoidController_->UpdateAvoidNodesMap(displayId, true);
    InitSysBarMapForDisplay(displayId);
    InitWindowNodeMapForDisplay(displayId);
    displayRectMap_.insert(std::make_pair(displayId, displayRect));
    layoutPolicy_->UpdateDisplayInfo(displayRectMap_);
}

void WindowNodeContainer::ProcessDisplayDestroy(DisplayId displayId, std::vector<uint32_t>& windowIds)
{
    WLOGFI("Process display destroy");
}

void WindowNodeContainer::ProcessDisplayChange(DisplayId displayId, const Rect& displayRect)
{
    displayRectMap_[displayId] = displayRect;
    layoutPolicy_->UpdateDisplayInfo(displayRectMap_);
}

void WindowNodeContainer::TraverseWindowTree(const WindowNodeOperationFunc& func, bool isFromTopToBottom) const
{
    std::vector<sptr<WindowNode>> rootNodes = { belowAppWindowNode_, appWindowNode_, aboveAppWindowNode_ };
    if (isFromTopToBottom) {
        std::reverse(rootNodes.begin(), rootNodes.end());
    }

    for (auto& node : rootNodes) {
        if (isFromTopToBottom) {
            for (auto iter = node->children_.rbegin(); iter != node->children_.rend(); ++iter) {
                if (TraverseFromTopToBottom(*iter, func)) {
                    return;
                }
            }
        } else {
            for (auto iter = node->children_.begin(); iter != node->children_.end(); ++iter) {
                if (TraverseFromBottomToTop(*iter, func)) {
                    return;
                }
            }
        }
    }
}

bool WindowNodeContainer::TraverseFromTopToBottom(sptr<WindowNode> node, const WindowNodeOperationFunc& func) const
{
    if (node == nullptr) {
        return false;
    }
    auto iterBegin = node->children_.rbegin();
    for (; iterBegin != node->children_.rend(); ++iterBegin) {
        if ((*iterBegin)->priority_ <= 0) {
            break;
        }
        if (func(*iterBegin)) {
            return true;
        }
    }
    if (func(node)) {
        return true;
    }
    for (; iterBegin != node->children_.rend(); ++iterBegin) {
        if (func(*iterBegin)) {
            return true;
        }
    }
    return false;
}

bool WindowNodeContainer::TraverseFromBottomToTop(sptr<WindowNode> node, const WindowNodeOperationFunc& func) const
{
    if (node == nullptr) {
        return false;
    }
    auto iterBegin = node->children_.begin();
    for (; iterBegin != node->children_.end(); ++iterBegin) {
        if ((*iterBegin)->priority_ >= 0) {
            break;
        }
        if (func(*iterBegin)) {
            return true;
        }
    }
    if (func(node)) {
        return true;
    }
    for (; iterBegin != node->children_.end(); ++iterBegin) {
        if (func(*iterBegin)) {
            return true;
        }
    }
    return false;
}

void WindowNodeContainer::UpdateWindowVisibilityInfos(std::vector<sptr<WindowVisibilityInfo>>& infos)
{
    auto emptyVector = std::vector<Rect>();
    currentCoveredArea_.swap(emptyVector);
    WindowNodeOperationFunc func = [this, &infos](sptr<WindowNode> node) {
        if (node == nullptr) {
            return false;
        }
        Rect layoutRect = node->GetWindowRect();
        const Rect& displayRect = displayRectMap_[node->GetDisplayId()];
        int32_t nodeX = std::max(0, layoutRect.posX_);
        int32_t nodeY = std::max(0, layoutRect.posY_);
        int32_t nodeXEnd = std::min(displayRect.posX_ + static_cast<int32_t>(displayRect.width_),
            layoutRect.posX_ + static_cast<int32_t>(layoutRect.width_));
        int32_t nodeYEnd = std::min(displayRect.posY_ + static_cast<int32_t>(displayRect.height_),
            layoutRect.posY_ + static_cast<int32_t>(layoutRect.height_));

        Rect rectInDisplay = {nodeX, nodeY,
                              static_cast<uint32_t>(nodeXEnd - nodeX), static_cast<uint32_t>(nodeYEnd - nodeY)};
        bool isCovered = false;
        for (auto& rect : currentCoveredArea_) {
            if (rectInDisplay.IsInsideOf(rect)) {
                isCovered = true;
                WLOGD("UpdateWindowVisibilityInfos: find covered window:%{public}u", node->GetWindowId());
                break;
            }
        }
        if (!isCovered) {
            currentCoveredArea_.emplace_back(rectInDisplay);
        }
        if (isCovered != node->isCovered_) {
            node->isCovered_ = isCovered;
            infos.emplace_back(new WindowVisibilityInfo(node->GetWindowId(), node->GetCallingPid(),
                node->GetCallingUid(), !isCovered));
            WLOGD("UpdateWindowVisibilityInfos: covered status changed window:%{public}u, covered:%{public}d",
                node->GetWindowId(), isCovered);
        }
        return false;
    };
    TraverseWindowTree(func, true);
    WindowManagerAgentController::GetInstance().UpdateWindowVisibilityInfo(infos);
}

float WindowNodeContainer::GetVirtualPixelRatio(DisplayId displayId) const
{
    return layoutPolicy_->GetVirtualPixelRatio(displayId);
}

bool WindowNodeContainer::ReadIsWindowAnimationEnabledProperty()
{
    if (access(DISABLE_WINDOW_ANIMATION_PATH, F_OK) == 0) {
        return false;
    }
    return true;
}

WMError WindowNodeContainer::SetWindowMode(sptr<WindowNode>& node, WindowMode dstMode)
{
    if (node == nullptr) {
        WLOGFE("could not find window");
        return WMError::WM_ERROR_NULLPTR;
    }
    WindowMode srcMode = node->GetWindowMode();
    if (srcMode == dstMode) {
        return WMError::WM_OK;
    }
    WMError res = WMError::WM_OK;
    if ((srcMode == WindowMode::WINDOW_MODE_FULLSCREEN) && (dstMode == WindowMode::WINDOW_MODE_FLOATING)) {
        node->SetWindowSizeChangeReason(WindowSizeChangeReason::RECOVER);
    } else if (dstMode == WindowMode::WINDOW_MODE_FULLSCREEN) {
        node->SetWindowSizeChangeReason(WindowSizeChangeReason::MAXIMIZE);
    } else {
        node->SetWindowSizeChangeReason(WindowSizeChangeReason::RESIZE);
    }
    node->SetWindowMode(dstMode);
    windowPair_->UpdateIfSplitRelated(node);
    if (node->GetWindowMode() == WindowMode::WINDOW_MODE_FULLSCREEN &&
        WindowHelper::IsAppWindow(node->GetWindowType())) {
        // minimize other app window
        res = MinimizeStructuredAppWindowsExceptSelf(node);
        if (res != WMError::WM_OK) {
            return res;
        }
    }
    if (node->GetWindowToken() != nullptr) {
        node->GetWindowToken()->UpdateWindowMode(node->GetWindowMode());
    }
    res = UpdateWindowNode(node, WindowUpdateReason::UPDATE_MODE);
    if (res != WMError::WM_OK) {
        WLOGFE("Set window mode failed, update node failed");
        return res;
    }
    return WMError::WM_OK;
}

void WindowNodeContainer::SetMinimizedByOther(bool isMinimizedByOther)
{
    isMinimizedByOther_ = isMinimizedByOther;
}

void WindowNodeContainer::GetModeChangeHotZones(DisplayId displayId, ModeChangeHotZones& hotZones,
    const ModeChangeHotZonesConfig& config)
{
    auto displayRect = displayRectMap_[displayId];

    hotZones.fullscreen_.width_ = displayRect.width_;
    hotZones.fullscreen_.height_ = config.fullscreenRange_;

    hotZones.primary_.width_ = config.primaryRange_;
    hotZones.primary_.height_ = displayRect.height_;

    hotZones.secondary_.posX_ = static_cast<int32_t>(displayRect.width_) - config.secondaryRange_;
    hotZones.secondary_.width_ = config.secondaryRange_;
    hotZones.secondary_.height_ = displayRect.height_;
}
} // namespace Rosen
} // namespace OHOS
