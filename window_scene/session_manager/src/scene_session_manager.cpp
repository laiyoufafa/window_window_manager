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

#include "session_manager/include/scene_session_manager.h"

#include <sstream>

#include <ability_info.h>
#include <ability_manager_client.h>
#include <bundle_mgr_interface.h>
#include <iservice_registry.h>
#include <parameters.h>
#include <resource_manager.h>
#include <session_info.h>
#include <start_options.h>
#include <system_ability_definition.h>
#include <want.h>

#include "color_parser.h"
#include "common/include/message_scheduler.h"
#include "common/include/permission.h"
#include "session/host/include/scene_session.h"
#include "window_manager_hilog.h"
#include "wm_math.h"

namespace OHOS::Rosen {
namespace {
constexpr HiviewDFX::HiLogLabel LABEL = { LOG_CORE, HILOG_DOMAIN_WINDOW, "SceneSessionManager" };
const std::string SCENE_SESSION_MANAGER_THREAD = "SceneSessionManager";
}

WM_IMPLEMENT_SINGLE_INSTANCE(SceneSessionManager)

SceneSessionManager::SceneSessionManager()
{
    Init();
}

void SceneSessionManager::Init()
{
    WLOGFI("scene session manager init");
    msgScheduler_ = std::make_shared<MessageScheduler>(SCENE_SESSION_MANAGER_THREAD);
    bundleMgr_ = GetBundleManager();
    LoadWindowSceneXml();
}

void SceneSessionManager::LoadWindowSceneXml()
{
    if (WindowSceneConfig::LoadConfigXml()) {
        if (WindowSceneConfig::GetConfig().IsMap()) {
            WindowSceneConfig::DumpConfig(*WindowSceneConfig::GetConfig().mapValue_);
        }
        ConfigWindowSceneXml();
    } else {
        WLOGFE("Load window scene xml failed");
    }
}

void SceneSessionManager::ConfigWindowSceneXml()
{
    const auto& config = WindowSceneConfig::GetConfig();
    WindowSceneConfig::ConfigItem item = config["windowEffect"];
    if (item.IsMap()) {
        ConfigWindowEffect(item);
    }
    item = config["decor"];
    if (item.IsMap()) {
        ConfigDecor(item);
    }
    item = config["defaultWindowMode"];
    if (item.IsInts()) {
        auto numbers = *item.intsValue_;
        if (numbers.size() == 1 &&
            (numbers[0] == static_cast<int32_t>(WindowMode::WINDOW_MODE_FULLSCREEN) ||
             numbers[0] == static_cast<int32_t>(WindowMode::WINDOW_MODE_FLOATING))) {
            systemConfig_.defaultWindowMode_ = static_cast<WindowMode>(static_cast<uint32_t>(numbers[0]));
        }
    }
}

void SceneSessionManager::ConfigDecor(const WindowSceneConfig::ConfigItem& decorConfig)
{
    WindowSceneConfig::ConfigItem item = decorConfig.GetProp("enable");
    if (item.IsBool()) {
        systemConfig_.isSystemDecorEnable_ = item.boolValue_;
        std::vector<std::string> supportedModes;
        item = decorConfig["supportedMode"];
        if (item.IsStrings()) {
            systemConfig_.decorModeSupportInfo_ = 0;
            supportedModes = *item.stringsValue_;
        }
        for (auto mode : supportedModes) {
            if (mode == "fullscreen") {
                systemConfig_.decorModeSupportInfo_ |= WindowModeSupport::WINDOW_MODE_SUPPORT_FULLSCREEN;
            } else if (mode == "floating") {
                systemConfig_.decorModeSupportInfo_ |= WindowModeSupport::WINDOW_MODE_SUPPORT_FLOATING;
            } else if (mode == "pip") {
                systemConfig_.decorModeSupportInfo_ |= WindowModeSupport::WINDOW_MODE_SUPPORT_PIP;
            } else if (mode == "split") {
                systemConfig_.decorModeSupportInfo_ |= WindowModeSupport::WINDOW_MODE_SUPPORT_SPLIT_PRIMARY |
                    WindowModeSupport::WINDOW_MODE_SUPPORT_SPLIT_SECONDARY;
            } else {
                WLOGFW("Invalid supporedMode");
                systemConfig_.decorModeSupportInfo_ = WindowModeSupport::WINDOW_MODE_SUPPORT_ALL;
                break;
            }
        }
    }
}

static void AddAlphaToColor(float alpha, std::string& color)
{
    if (color.size() == 9 || alpha > 1.0f) { // size 9: color is ARGB
        return;
    }

    uint32_t alphaValue = 0xFF * alpha;
    std::stringstream ss;
    ss << std::hex << alphaValue;
    std::string strAlpha = ss.str();
    if (strAlpha.size() == 1) {
        strAlpha.append(1, '0');
    }

    color.insert(1, strAlpha);
}

void SceneSessionManager::ConfigWindowEffect(const WindowSceneConfig::ConfigItem& effectConfig)
{
    AppWindowSceneConfig config;
    // config corner radius
    WindowSceneConfig::ConfigItem item = effectConfig["appWindows"]["cornerRadius"];
    if (item.IsMap()) {
        if (ConfigAppWindowCornerRadius(item["float"], config.floatCornerRadius_)) {
            appWindowSceneConfig_ = config;
        }
    }

    // config shadow
    item = effectConfig["appWindows"]["shadow"]["focused"];
    if (item.IsMap()) {
        if (ConfigAppWindowShadow(item, config.focusedShadow_)) {
            appWindowSceneConfig_.focusedShadow_ = config.focusedShadow_;
        }
    }

    item = effectConfig["appWindows"]["shadow"]["unfocused"];
    if (item.IsMap()) {
        if (ConfigAppWindowShadow(item, config.unfocusedShadow_)) {
            appWindowSceneConfig_.unfocusedShadow_ = config.unfocusedShadow_;
        }
    }

    AddAlphaToColor(appWindowSceneConfig_.focusedShadow_.alpha_, appWindowSceneConfig_.focusedShadow_.color_);
    AddAlphaToColor(appWindowSceneConfig_.unfocusedShadow_.alpha_, appWindowSceneConfig_.unfocusedShadow_.color_);

    WLOGFI("Config window effect successfully");
}

bool SceneSessionManager::ConfigAppWindowCornerRadius(const WindowSceneConfig::ConfigItem& item, float& out)
{
    std::map<std::string, float> stringToCornerRadius = {
        {"off", 0.0f}, {"defaultCornerRadiusXS", 4.0f}, {"defaultCornerRadiusS", 8.0f},
        {"defaultCornerRadiusM", 12.0f}, {"defaultCornerRadiusL", 16.0f}, {"defaultCornerRadiusXL", 24.0f}
    };

    if (item.IsString()) {
        auto value = item.stringValue_;
        if (stringToCornerRadius.find(value) != stringToCornerRadius.end()) {
            out = stringToCornerRadius[value];
            return true;
        }
    }
    return false;
}

bool SceneSessionManager::ConfigAppWindowShadow(const WindowSceneConfig::ConfigItem& shadowConfig,
    WindowShadowConfig& outShadow)
{
    WindowSceneConfig::ConfigItem item = shadowConfig["color"];
    if (item.IsString()) {
        auto color = item.stringValue_;
        uint32_t colorValue;
        if (!ColorParser::Parse(color, colorValue)) {
            return false;
        }
        outShadow.color_ = color;
    }

    item = shadowConfig["offsetX"];
    if (item.IsFloats()) {
        auto offsetX = *item.floatsValue_;
        if (offsetX.size() != 1) {
            return false;
        }
        outShadow.offsetX_ = offsetX[0];
    }

    item = shadowConfig["offsetY"];
    if (item.IsFloats()) {
        auto offsetY = *item.floatsValue_;
        if (offsetY.size() != 1) {
            return false;
        }
        outShadow.offsetY_ = offsetY[0];
    }

    item = shadowConfig["alpha"];
    if (item.IsFloats()) {
        auto alpha = *item.floatsValue_;
        if (alpha.size() != 1 ||
            (MathHelper::LessNotEqual(alpha[0], 0.0) && MathHelper::GreatNotEqual(alpha[0], 1.0))) {
            return false;
        }
        outShadow.alpha_ = alpha[0];
    }

    item = shadowConfig["radius"];
    if (item.IsFloats()) {
        auto radius = *item.floatsValue_;
        if (radius.size() != 1 || MathHelper::LessNotEqual(radius[0], 0.0)) {
            return false;
        }
        outShadow.radius_ = radius[0];
    }

    return true;
}

sptr<RootSceneSession> SceneSessionManager::GetRootSceneSession()
{
    auto task = [this]() {
        if (rootSceneSession_ != nullptr) {
            return rootSceneSession_;
        }
        system::SetParameter("bootevent.boot.completed", "true");
        SessionInfo info;
        rootSceneSession_ = new (std::nothrow) RootSceneSession(info);
        if (!rootSceneSession_) {
            WLOGFE("rootSceneSession is nullptr");
            return sptr<RootSceneSession>(nullptr);
        }
        sptr<ISession> iSession(rootSceneSession_);
        AAFwk::AbilityManagerClient::GetInstance()->SetRootSceneSession(iSession->AsObject());
        return rootSceneSession_;
    };

    WS_CHECK_NULL_SCHE_RETURN(msgScheduler_, task);
    return msgScheduler_->PostSyncTask(task);
}

sptr<SceneSession> SceneSessionManager::GetSceneSession(uint64_t persistentId)
{
    auto iter = abilitySceneMap_.find(persistentId);
    if (iter == abilitySceneMap_.end()) {
        WLOGFE("Error found scene session with id: %{public}" PRIu64, persistentId);
        return nullptr;
    }
    return iter->second;
}

sptr<SceneSession> SceneSessionManager::RequestSceneSession(const SessionInfo& sessionInfo)
{
    sptr<SceneSession::SpecificSessionCallback> specificCallback = new (std::nothrow)
        SceneSession::SpecificSessionCallback();
    if (specificCallback == nullptr) {
        WLOGFE("SpecificSessionCallback is nullptr");
        return nullptr;
    }
    specificCallback->onCreate_ = std::bind(&SceneSessionManager::RequestSceneSession,
        this, std::placeholders::_1);
    specificCallback->onDestroy_ = std::bind(&SceneSessionManager::DestroyAndDisconnectSpecificSession,
        this, std::placeholders::_1);
    auto task = [this, sessionInfo, specificCallback]() {
        WLOGFI("sessionInfo: bundleName: %{public}s, moduleName: %{public}s, abilityName: %{public}s",
            sessionInfo.bundleName_.c_str(), sessionInfo.moduleName_.c_str(), sessionInfo.abilityName_.c_str());
        sptr<SceneSession> sceneSession = new (std::nothrow) SceneSession(sessionInfo, specificCallback);
        if (sceneSession == nullptr) {
            WLOGFE("sceneSession is nullptr!");
            return sceneSession;
        }
        auto persistentId = sceneSession->GetPersistentId();
        sceneSession->SetSystemConfig(systemConfig_);
        // set parent session to sub session
        if (sceneSession->GetWindowType() == WindowType::WINDOW_TYPE_APP_SUB_WINDOW) {
            auto parentId = sceneSession->GetParentPersistentId();
            if (!abilitySceneMap_.count(parentId)) {
                WLOGFD("session is invalid");
            } else {
                sceneSession->SetParentSession(abilitySceneMap_.at(parentId));
            }
        }
        abilitySceneMap_.insert({ persistentId, sceneSession });
        WLOGFI("create session persistentId: %{public}" PRIu64 "", persistentId);
        return sceneSession;
    };
    WS_CHECK_NULL_SCHE_RETURN(msgScheduler_, task);
    return msgScheduler_->PostSyncTask(task);
}

sptr<AAFwk::SessionInfo> SceneSessionManager::SetAbilitySessionInfo(const sptr<SceneSession>& scnSession)
{
    sptr<AAFwk::SessionInfo> abilitySessionInfo = new (std::nothrow) AAFwk::SessionInfo();
    if (!abilitySessionInfo) {
        WLOGFE("abilitySessionInfo is nullptr");
        return nullptr;
    }
    auto sessionInfo = scnSession->GetSessionInfo();
    sptr<ISession> iSession(scnSession);
    abilitySessionInfo->sessionToken = iSession->AsObject();
    abilitySessionInfo->callerToken = sessionInfo.callerToken_;
    abilitySessionInfo->persistentId = scnSession->GetPersistentId();
    return abilitySessionInfo;
}

WSError SceneSessionManager::RequestSceneSessionActivation(const sptr<SceneSession>& sceneSession)
{
    wptr<SceneSession> weakSceneSession(sceneSession);
    auto task = [this, weakSceneSession]() {
        auto scnSession = weakSceneSession.promote();
        if (scnSession == nullptr) {
            WLOGFE("session is nullptr");
            return WSError::WS_ERROR_NULLPTR;
        }
        auto persistentId = scnSession->GetPersistentId();
        WLOGFI("active persistentId: %{public}" PRIu64 "", persistentId);
        if (abilitySceneMap_.count(persistentId) == 0) {
            WLOGFE("session is invalid with %{public}" PRIu64 "", persistentId);
            return WSError::WS_ERROR_INVALID_SESSION;
        }
        AAFwk::Want want;
        auto sessionInfo = scnSession->GetSessionInfo();
        want.SetElementName("", sessionInfo.bundleName_, sessionInfo.abilityName_, sessionInfo.moduleName_);
        AAFwk::StartOptions startOptions;
        auto scnSessionInfo = SetAbilitySessionInfo(scnSession);
        if (!scnSessionInfo) {
            return WSError::WS_ERROR_NULLPTR;
        }
        AAFwk::AbilityManagerClient::GetInstance()->StartUIAbilityBySCB(want, startOptions, scnSessionInfo);
        activeSessionId_ = persistentId;
        return WSError::WS_OK;
    };
    WS_CHECK_NULL_SCHE_RETURN(msgScheduler_, task);
    msgScheduler_->PostAsyncTask(task);
    return WSError::WS_OK;
}

WSError SceneSessionManager::RequestSceneSessionBackground(const sptr<SceneSession>& sceneSession)
{
    wptr<SceneSession> weakSceneSession(sceneSession);
    auto task = [this, weakSceneSession]() {
        auto scnSession = weakSceneSession.promote();
        if (scnSession == nullptr) {
            WLOGFE("session is nullptr");
            return WSError::WS_ERROR_NULLPTR;
        }
        auto persistentId = scnSession->GetPersistentId();
        WLOGFI("background session persistentId: %{public}" PRIu64 "", persistentId);
        scnSession->SetActive(false);
        scnSession->Background();
        if (abilitySceneMap_.count(persistentId) == 0) {
            WLOGFE("session is invalid with %{public}" PRIu64 "", persistentId);
            return WSError::WS_ERROR_INVALID_SESSION;
        }
        auto scnSessionInfo = SetAbilitySessionInfo(scnSession);
        if (!scnSessionInfo) {
            return WSError::WS_ERROR_NULLPTR;
        }
        AAFwk::AbilityManagerClient::GetInstance()->MinimizeUIAbilityBySCB(scnSessionInfo);
        return WSError::WS_OK;
    };

    WS_CHECK_NULL_SCHE_RETURN(msgScheduler_, task);
    msgScheduler_->PostAsyncTask(task);
    return WSError::WS_OK;
}

WSError SceneSessionManager::DestroyDialogWithMainWindow(const sptr<SceneSession>& scnSession)
{
    if (scnSession->GetWindowType() == WindowType::WINDOW_TYPE_APP_MAIN_WINDOW) {
        WLOGFD("Begin to destroy its dialog");
        auto dialogVec = scnSession->GetDialogVector();
        for (auto dialog : dialogVec) {
            if (abilitySceneMap_.count(dialog->GetPersistentId()) == 0) {
                WLOGFE("session is invalid with %{public}" PRIu64 "", dialog->GetPersistentId());
                return WSError::WS_ERROR_INVALID_SESSION;
            }
            dialog->NotifyDestroy();
            dialog->Disconnect();
            abilitySceneMap_.erase(dialog->GetPersistentId());
        }
        return WSError::WS_OK;
    }
    return WSError::WS_ERROR_INVALID_SESSION;
}

WSError SceneSessionManager::RequestSceneSessionDestruction(const sptr<SceneSession>& sceneSession)
{
    wptr<SceneSession> weakSceneSession(sceneSession);
    auto task = [this, weakSceneSession]() {
        auto scnSession = weakSceneSession.promote();
        if (scnSession == nullptr) {
            WLOGFE("session is nullptr");
            return WSError::WS_ERROR_NULLPTR;
        }
        auto persistentId = scnSession->GetPersistentId();
        DestroyDialogWithMainWindow(scnSession);
        WLOGFI("destroy session persistentId: %{public}" PRIu64 "", persistentId);
        scnSession->Disconnect();
        if (abilitySceneMap_.count(persistentId) == 0) {
            WLOGFE("session is invalid with %{public}" PRIu64 "", persistentId);
            return WSError::WS_ERROR_INVALID_SESSION;
        }
        abilitySceneMap_.erase(persistentId);
        auto scnSessionInfo = SetAbilitySessionInfo(scnSession);
        if (!scnSessionInfo) {
            return WSError::WS_ERROR_NULLPTR;
        }
        AAFwk::AbilityManagerClient::GetInstance()->CloseUIAbilityBySCB(scnSessionInfo);
        return WSError::WS_OK;
    };

    WS_CHECK_NULL_SCHE_RETURN(msgScheduler_, task);
    msgScheduler_->PostAsyncTask(task);
    return WSError::WS_OK;
}

WSError SceneSessionManager::CreateAndConnectSpecificSession(const sptr<ISessionStage>& sessionStage,
    const sptr<IWindowEventChannel>& eventChannel, const std::shared_ptr<RSSurfaceNode>& surfaceNode,
    sptr<WindowSessionProperty> property, uint64_t& persistentId, sptr<ISession>& session)
{
    if (!Permission::IsSystemCalling() && !Permission::IsStartedByInputMethod()) {
        WLOGFE("check input method permission failed");
    }
    auto task = [this, sessionStage, eventChannel, surfaceNode, property, &persistentId, &session]() {
        // create specific session
        SessionInfo info;
        sptr<SceneSession> sceneSession = RequestSceneSession(info);
        if (sceneSession == nullptr) {
            return WSError::WS_ERROR_NULLPTR;
        }
        // connect specific session and sessionStage
        WSError errCode = sceneSession->Connect(sessionStage, eventChannel, surfaceNode, systemConfig_, property);
        if (property) {
            persistentId = property->GetPersistentId();
        }
        if (createSpecificSessionFunc_) {
            createSpecificSessionFunc_(sceneSession);
        }
        // when create dialog, bind to its host
        if (sceneSession->GetWindowType() == WindowType::WINDOW_TYPE_DIALOG &&
            sceneSession->GetParentPersistentId() != INVALID_SESSION_ID) {
            auto parentSession = GetSceneSession(sceneSession->GetParentPersistentId());
            if (parentSession) {
                WLOGFD("Add dialog id to its parent vector");
                parentSession->BindDialogToParentSession(sceneSession);
                sceneSession->SetParentSession(parentSession);
            }
        }
        session = sceneSession;
        return errCode;
    };
    WS_CHECK_NULL_SCHE_RETURN(msgScheduler_, task);
    msgScheduler_->PostSyncTask(task);
    return WSError::WS_OK;
}

void SceneSessionManager::SetCreateSpecificSessionListener(const NotifyCreateSpecificSessionFunc& func)
{
    WLOGFD("SetCreateSpecificSessionListener");
    createSpecificSessionFunc_ = func;
}

WSError SceneSessionManager::DestroyAndDisconnectSpecificSession(const uint64_t& persistentId)
{
    auto task = [this, persistentId]() {
        WLOGFI("Destroy session persistentId: %{public}" PRIu64 "", persistentId);
        auto iter = abilitySceneMap_.find(persistentId);
        if (iter == abilitySceneMap_.end()) {
            return WSError::WS_ERROR_INVALID_SESSION;
        }
        const auto& sceneSession = iter->second;
        if (sceneSession == nullptr) {
            return WSError::WS_ERROR_NULLPTR;
        }
        auto ret = sceneSession->UpdateActiveStatus(false);
        if (sceneSession->GetWindowType() == WindowType::WINDOW_TYPE_DIALOG) {
            auto parentSession = GetSceneSession(sceneSession->GetParentPersistentId());
            parentSession->RemoveDialogToParentSession(sceneSession);
            sceneSession->NotifyDestroy();
        }
        ret = sceneSession->Disconnect();
        abilitySceneMap_.erase(persistentId);
        return ret;
    };

    WS_CHECK_NULL_SCHE_RETURN(msgScheduler_, task);
    msgScheduler_->PostSyncTask(task);
    return WSError::WS_OK;
}

const AppWindowSceneConfig& SceneSessionManager::GetWindowSceneConfig() const
{
    return appWindowSceneConfig_;
}

WSError SceneSessionManager::ProcessBackEvent()
{
    auto task = [this]() {
        auto session = GetSceneSession(activeSessionId_);
        if (!session) {
            return WSError::WS_ERROR_INVALID_SESSION;
        }
        WLOGFD("ProcessBackEvent session persistentId: %{public}" PRIu64 "", activeSessionId_);
        session->ProcessBackEvent();
        return WSError::WS_OK;
    };

    WS_CHECK_NULL_SCHE_RETURN(msgScheduler_, task);
    msgScheduler_->PostSyncTask(task);
    return WSError::WS_OK;
}

sptr<AppExecFwk::IBundleMgr> SceneSessionManager::GetBundleManager()
{
    auto systemAbilityMgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (systemAbilityMgr == nullptr) {
        WLOGFE("Failed to get SystemAbilityManager.");
        return nullptr;
    }

    auto bmsObj = systemAbilityMgr->GetSystemAbility(BUNDLE_MGR_SERVICE_SYS_ABILITY_ID);
    if (bmsObj == nullptr) {
        WLOGFE("Failed to get BundleManagerService.");
        return nullptr;
    }

    return iface_cast<AppExecFwk::IBundleMgr>(bmsObj);
}

std::shared_ptr<Global::Resource::ResourceManager> SceneSessionManager::CreateResourceManager(
    const AppExecFwk::AbilityInfo& abilityInfo)
{
    std::unique_ptr<Global::Resource::ResConfig> resConfig(Global::Resource::CreateResConfig());
    std::shared_ptr<Global::Resource::ResourceManager> resourceMgr(Global::Resource::CreateResourceManager());
    resourceMgr->UpdateResConfig(*resConfig);

    std::string loadPath;
    if (!abilityInfo.hapPath.empty()) { // zipped hap
        loadPath = abilityInfo.hapPath;
    } else {
        loadPath = abilityInfo.resourcePath;
    }

    if (!resourceMgr->AddResource(loadPath.c_str())) {
        WLOGFE("Add resource %{private}s failed.", loadPath.c_str());
        return nullptr;
    }
    return resourceMgr;
}

void SceneSessionManager::GetStartPageFromResource(const AppExecFwk::AbilityInfo& abilityInfo,
    std::string& path, uint32_t& bgColor)
{
    auto resourceMgr = CreateResourceManager(abilityInfo);
    if (resourceMgr == nullptr) {
        WLOGFE("resource manager is nullptr.");
        return;
    }

    if (resourceMgr->GetColorById(abilityInfo.startWindowBackgroundId, bgColor) != Global::Resource::RState::SUCCESS) {
        WLOGFW("Failed to get background color id %{private}d.", abilityInfo.startWindowBackgroundId);
    }

    if (resourceMgr->GetMediaById(abilityInfo.startWindowIconId, path) != Global::Resource::RState::SUCCESS) {
        WLOGFE("Failed to get icon id %{private}d.", abilityInfo.startWindowIconId);
        return;
    }

    if (!abilityInfo.hapPath.empty()) { // zipped hap
        auto pos = path.find_last_of('.');
        if (pos == std::string::npos) {
            WLOGFE("Format error, path %{private}s.", path.c_str());
            return;
        }
        path = "resource:///" + std::to_string(abilityInfo.startWindowIconId) + path.substr(pos);
    }
}

void SceneSessionManager::GetStartPage(const SessionInfo& sessionInfo, std::string& path, uint32_t& bgColor)
{
    if (!bundleMgr_) {
        WLOGFE("bundle manager is nullptr.");
        return;
    }

    AAFwk::Want want;
    want.SetElementName("", sessionInfo.bundleName_, sessionInfo.abilityName_, sessionInfo.moduleName_);
    AppExecFwk::AbilityInfo abilityInfo;
    bool ret = bundleMgr_->QueryAbilityInfo(
        want, AppExecFwk::GET_ABILITY_INFO_DEFAULT, AppExecFwk::Constants::ANY_USERID, abilityInfo);
    if (!ret) {
        WLOGFE("Get ability info from BMS failed!");
        return;
    }

    GetStartPageFromResource(abilityInfo, path, bgColor);
}

WSError SceneSessionManager::UpdateProperty(sptr<WindowSessionProperty>& property, WSPropertyChangeAction action)
{
    if (property == nullptr) {
        WLOGFE("property is invalid");
        return WSError::WS_ERROR_NULLPTR;
    }
    uint64_t persistentId = property->GetPersistentId();
    auto sceneSession = GetSceneSession(persistentId);
    if (sceneSession == nullptr) {
        WLOGFE("session is invalid");
        return WSError::WS_ERROR_NULLPTR;
    }
    WLOGI("Id: %{public}" PRIu64", action: %{public}u", sceneSession->GetPersistentId(), static_cast<uint32_t>(action));
    WSError ret = WSError::WS_OK;
    switch (action) {
        case WSPropertyChangeAction::ACTION_UPDATE_FLAGS: {
            // @todo
            break;
        }
        case WSPropertyChangeAction::ACTION_UPDATE_FOCUSABLE: {
            sceneSession->SetFocusable(property->GetFocusable());
            break;
        }
        case WSPropertyChangeAction::ACTION_UPDATE_TOUCHABLE: {
            // @todo
            break;
        }
        case WSPropertyChangeAction::ACTION_UPDATE_SET_BRIGHTNESS: {
            // @todo
            break;
        }
        case WSPropertyChangeAction::ACTION_UPDATE_PRIVACY_MODE: {
            // @todo
            break;
        }
        default:
            break;
    }

    return ret;
}

WSError SceneSessionManager::SetFocusedSession(uint64_t persistentId)
{
    if (focusedSessionId_ == persistentId) {
        WLOGI("Focus scene not change, id: %{public}" PRIu64, focusedSessionId_);
        return WSError::WS_DO_NOTHING;
    }
    focusedSessionId_ = persistentId;
    return WSError::WS_OK;
}

uint64_t SceneSessionManager::GetFocusedSession() const
{
    return focusedSessionId_;
}

WSError SceneSessionManager::UpdateFocus(uint64_t persistentId, bool isFocused)
{
    // notify session and client
    auto sceneSession = GetSceneSession(persistentId);
    if (sceneSession == nullptr) {
        WLOGFE("could not find window");
        return WSError::WS_ERROR_INVALID_WINDOW;
    }
    WSError res = WSError::WS_OK;
    res = sceneSession->UpdateFocus(isFocused);
    if (res != WSError::WS_OK) {
        return res;
    }
    // focusId change
    if (isFocused) {
        return SetFocusedSession(persistentId);
    }
    if (persistentId == GetFocusedSession()) {
        focusedSessionId_ = INVALID_SESSION_ID;
    }
    return WSError::WS_OK;
}
} // namespace OHOS::Rosen
