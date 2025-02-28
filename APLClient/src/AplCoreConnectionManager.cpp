/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <climits>

#include "APLClient/AplCoreTextMeasurement.h"
#include "APLClient/AplCoreLocaleMethods.h"
#include "APLClient/AplCoreConnectionManager.h"
#include "APLClient/AplCoreViewhostMessage.h"
#include "APLClient/AplCoreAudioPlayerFactory.h"
#include "APLClient/AplCoreMediaPlayerFactory.h"
#include "APLClient/Extensions/AplCoreExtensionExecutor.h"

#include <apl/datasource/dynamicindexlistdatasourceprovider.h>
#include <apl/datasource/dynamictokenlistdatasourceprovider.h>
#include <apl/content/rootproperties.h>
#include <apl/component/videocomponent.h>

namespace APLClient {

/// CDN for alexa import packages (styles/resources/etc)
/// (https://developer.amazon.com/en-US/docs/alexa/alexa-presentation-language/apl-document.html#import)
static const char* ALEXA_IMPORT_PATH = "https://arl.assets.apl-alexa.com/packages/%s/%s/document.json";
/// The number of bytes read from the attachment with each read in the read loop.
static const size_t CHUNK_SIZE(1024);

/// The keys used in ProvideState.
static const char TOKEN_KEY[] = "token";
static const char VERSION_KEY[] = "version";
static const char VISUAL_CONTEXT_KEY[] = "componentsVisibleOnScreen";
static const char DATASOURCE_CONTEXT_KEY[] = "dataSources";
/// The value used in ProvideState.
static const char CLIENT_VERSION_PREFIX[] = "AplClientLibrary-";

// Key used in messaging
static const char SEQNO_KEY[] = "seqno";

/// APL Scaling bias constant
static const float SCALING_BIAS_CONSTANT = 10.0f;
/// APL Scaling cost override
static const bool SCALING_SHAPE_OVERRIDES_COST = true;

/// The keys used in APL context creation.
static const char HEIGHT_KEY[] = "height";
static const char WIDTH_KEY[] = "width";
static const char DPI_KEY[] = "dpi";
static const char MODE_KEY[] = "mode";
static const char SHAPE_KEY[] = "shape";
static const char SCALING_KEY[] = "scaling";
static const char SCALE_FACTOR_KEY[] = "scaleFactor";
static const char VIEWPORT_WIDTH_KEY[] = "viewportWidth";
static const char VIEWPORT_HEIGHT_KEY[] = "viewportHeight";
static const char HIERARCHY_KEY[] = "hierarchy";
static const char REHIERARCHY_KEY[] = "reHierarchy";
static const char X_KEY[] = "x";
static const char Y_KEY[] = "y";
static const char DOCTHEME_KEY[] = "docTheme";
static const char BACKGROUND_KEY[] = "background";
static const char SCREENLOCK_KEY[] = "screenLock";
static const char COLOR_KEY[] = "color";
static const char GRADIENT_KEY[] = "gradient";
static const char ENSURELAYOUT_KEY[] = "ensureLayout";
static const char AGENTNAME_KEY[] = "agentName";
static const char AGENTVERSION_KEY[] = "agentVersion";
static const char ALLOWOPENURL_KEY[] = "allowOpenUrl";
static const char DISALLOWVIDEO_KEY[] = "disallowVideo";
static const char DISALLOWDIALOG_KEY[] = "disallowDialog";
static const char DISALLOWEDITTEXT_KEY[] = "disallowEditText";
static const char ANIMATIONQUALITY_KEY[] = "animationQuality";
static const char SUPPORTED_EXTENSIONS[] = "supportedExtensions";
static const char EXTENSION_MESSAGE_KEY[] = "extension";
static const char SCROLL_COMMAND_DURATION_KEY[] = "scrollCommandDuration";

/// The keys used to provide SupportedExtensions from JS
static const char URI_KEY[] = "uri";
static const char FLAGS_KEY[] = "flags";

/// The keys used in OS accessibility settings.
static const char FONTSCALE_KEY[] = "fontScale";
static const char SCREENMODE_KEY[] = "screenMode";
static const char SCREENREADER_KEY[] = "screenReader";

/// Document settings keys.
static const char SUPPORTS_RESIZING_KEY[] = "supportsResizing";
static const char ENVIRONMENT_VALUE_KEY[] = "environmentValues";

/// The keys used in APL event execution.
static const char ERROR_KEY[] = "error";
static const char EVENT_KEY[] = "event";
static const char ARGUMENT_KEY[] = "argument";
static const char EVENT_TERMINATE_KEY[] = "eventTerminate";
static const char DIRTY_KEY[] = "dirty";

/// SendEvent keys
static const char PRESENTATION_TOKEN_KEY[] = "presentationToken";
static const char SOURCE_KEY[] = "source";
static const char ARGUMENTS_KEY[] = "arguments";
static const char COMPONENTS_KEY[] = "components";

/// RuntimeError keys
static const char ERRORS_KEY[] = "errors";

/// Media update keys
static const char MEDIA_STATE_KEY[] = "mediaState";
static const char FROM_EVENT_KEY[] = "fromEvent";
static const char TRACK_INDEX_KEY[] = "trackIndex";
static const char TRACK_COUNT_KEY[] = "trackCount";
static const char TRACK_STATE_KEY[] = "trackState";
static const char CURRENT_TIME_KEY[] = "currentTime";
static const char DURATION_KEY[] = "duration";
static const char PAUSED_KEY[] = "paused";
static const char ENDED_KEY[] = "ended";
static const char MUTED_KEY[] = "muted";

/// Activity tracking sources
static const std::string APL_COMMAND_EXECUTION{"APLCommandExecution"};
static const std::string APL_SCREEN_LOCK{"APLScreenLock"};
static const char RENDERING_OPTIONS_KEY[] = "renderingOptions";

static const char LEGACY_KARAOKE_KEY[] = "legacyKaraoke";
static const char DOCUMENT_APL_VERSION_KEY[] = "documentAplVersion";

/// HandlePointerEvent keys
static const char POINTEREVENTTYPE_KEY[] = "pointerEventType";
static const char POINTERTYPE_KEY[] = "pointerType";
static const char POINTERID_KEY[] = "pointerId";

// Default font
static const char DEFAULT_FONT[] = "amazon-ember-display";

/// Data sources
static const std::vector<std::string> KNOWN_DATA_SOURCES = {
        apl::DynamicIndexListConstants::DEFAULT_TYPE_NAME,
        apl::DynamicTokenListConstants::DEFAULT_TYPE_NAME,
};

static apl::Bimap<std::string, apl::ViewportMode> AVS_VIEWPORT_MODE_MAP = {
        {"HUB",    apl::ViewportMode::kViewportModeHub},
        {"TV",     apl::ViewportMode::kViewportModeTV},
        {"MOBILE", apl::ViewportMode::kViewportModeMobile},
        {"AUTO",   apl::ViewportMode::kViewportModeAuto},
        {"PC",     apl::ViewportMode::kViewportModePC},
};

static apl::Bimap<std::string, apl::ScreenShape> AVS_VIEWPORT_SHAPE_MAP = {
        {"ROUND",     apl::ScreenShape::ROUND},
        {"RECTANGLE", apl::ScreenShape::RECTANGLE},
};

static apl::Bimap<std::string, apl::RootConfig::ScreenMode> AVS_SCREEN_MODE_MAP = {
        {"normal",        apl::RootConfig::kScreenModeNormal},
        {"high-contrast", apl::RootConfig::kScreenModeHighContrast},
};

AplCoreConnectionManager::AplCoreConnectionManager(AplConfigurationPtr config) :
        m_aplConfiguration{config},
        m_ScreenLock{false},
        m_SequenceNumber{0},
        m_replyExpectedSequenceNumber{0},
        m_blockingSendReplyExpected{false} {
    m_StartTime = getCurrentTime();

    m_extensionManager = std::make_shared<AplCoreExtensionManager>();
    m_messageHandlers.emplace("build", [this](const rapidjson::Value& payload) { handleBuild(payload); });
    m_messageHandlers.emplace("configurationChange",[this](const rapidjson::Value& payload) { handleConfigurationChange(payload); });
    m_messageHandlers.emplace("updateDisplayState", [this](const rapidjson::Value& payload) { handleUpdateDisplayState(payload); });
    m_messageHandlers.emplace("update", [this](const rapidjson::Value& payload) { handleUpdate(payload); });
    m_messageHandlers.emplace("updateMedia", [this](const rapidjson::Value& payload) { handleMediaUpdate(payload); });
    m_messageHandlers.emplace("updateGraphic", [this](const rapidjson::Value& payload) { handleGraphicUpdate(payload); });
    m_messageHandlers.emplace("response", [this](const rapidjson::Value& payload) { handleEventResponse(payload); });
    m_messageHandlers.emplace("ensureLayout", [this](const rapidjson::Value& payload) { handleEnsureLayout(payload); });
    m_messageHandlers.emplace("scrollToRectInComponent",[this](const rapidjson::Value& payload) { handleScrollToRectInComponent(payload); });
    m_messageHandlers.emplace("handleKeyboard", [this](const rapidjson::Value& payload) { handleHandleKeyboard(payload); });
    m_messageHandlers.emplace("getFocusableAreas", [this](const rapidjson::Value& payload) { getFocusableAreas(payload); });
    m_messageHandlers.emplace("getFocused", [this](const rapidjson::Value& payload) { getFocused(payload); });
    m_messageHandlers.emplace("getVisualContext", [this](const rapidjson::Value& payload) { getVisualContext(payload); });
    m_messageHandlers.emplace("getDataSourceContext", [this](const rapidjson::Value& payload) { getDataSourceContext(payload); });
    m_messageHandlers.emplace("setFocus", [this](const rapidjson::Value& payload) { setFocus(payload); });
    m_messageHandlers.emplace("updateCursorPosition", [this](const rapidjson::Value& payload) { handleUpdateCursorPosition(payload); });
    m_messageHandlers.emplace("handlePointerEvent", [this](const rapidjson::Value& payload) { handleHandlePointerEvent(payload); });
    m_messageHandlers.emplace("isCharacterValid", [this](const rapidjson::Value& payload) { handleIsCharacterValid(payload); });
    m_messageHandlers.emplace("reInflate", [this](const rapidjson::Value& payload) { handleReInflate(payload); });
    m_messageHandlers.emplace("reHierarchy", [this](const rapidjson::Value& payload) { handleReHierarchy(payload); });
    m_messageHandlers.emplace("extension", [this](const rapidjson::Value& payload) { handleExtensionMessage(payload); });
    m_messageHandlers.emplace("mediaLoaded", [this](const rapidjson::Value& payload) { mediaLoaded(payload); });
    m_messageHandlers.emplace("mediaLoadFailed", [this](const rapidjson::Value& payload) { mediaLoadFailed(payload); });
    m_messageHandlers.emplace("audioPlayerCallback", [this](const rapidjson::Value& payload) { audioPlayerCallback(payload); });
    m_messageHandlers.emplace("speechMarkCallback", [this](const rapidjson::Value& payload) { audioPlayerSpeechMarks(payload); });
    m_messageHandlers.emplace("mediaPlayerUpdateMediaState", [this](const rapidjson::Value& payload) { mediaPlayerUpdateMediaState(payload); });
    m_messageHandlers.emplace("mediaPlayerDoCallback", [this](const rapidjson::Value& payload) { mediaPlayerDoCallback(payload); });
}

void AplCoreConnectionManager::setContent(const apl::ContentPtr content, const std::string& token) {
    m_Content = content;
    m_aplToken = token;
    m_ConfigurationChange.clear();
    m_aplConfiguration->getAplOptions()->resetViewhost(token);
}

void AplCoreConnectionManager::setSupportedViewports(const std::string& jsonPayload) {
    rapidjson::Document doc;
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (doc.Parse(jsonPayload.c_str()).HasParseError()) {
        aplOptions->logMessage(LogLevel::ERROR, "setSupportedViewportsFailed", "Failed to parse json payload");
        return;
    }

    if (doc.GetType() != rapidjson::Type::kArrayType) {
        aplOptions->logMessage(LogLevel::ERROR, "setSupportedViewportsFailed", "Unexpected json document type");
        return;
    }

    m_ViewportSizeSpecifications.clear();
    for (auto& spec : doc.GetArray()) {
        double minWidth = getOptionalValue(spec, "minWidth", 1);
        double maxWidth = getOptionalValue(spec, "maxWidth", INT_MAX);
        double minHeight = getOptionalValue(spec, "minHeight", 1);
        double maxHeight = getOptionalValue(spec, "maxHeight", INT_MAX);
        std::string mode = getOptionalValue(spec, "mode", "HUB");
        std::string shape = spec.FindMember("shape")->value.GetString();
        // Ensure the viewport mode is uppercased
        std::transform(mode.begin(), mode.end(), mode.begin(),
            [](unsigned char c){ return std::toupper(c); });

        m_ViewportSizeSpecifications.emplace_back(
                minWidth,
                maxWidth,
                minHeight,
                maxHeight,
                AVS_VIEWPORT_MODE_MAP.at(mode),
                AVS_VIEWPORT_SHAPE_MAP.at(shape) == apl::ScreenShape::ROUND);
    }
}

double AplCoreConnectionManager::getOptionalValue(
        const rapidjson::Value& jsonNode,
        const std::string& key,
        double defaultValue) {
    double value = defaultValue;
    const auto& valueIt = jsonNode.FindMember(key);
    if (valueIt != jsonNode.MemberEnd()) {
        value = valueIt->value.GetDouble();
    }

    return value;
}

std::string AplCoreConnectionManager::getOptionalValue(
        const rapidjson::Value& jsonNode,
        const std::string& key,
        const std::string& defaultValue) {
    std::string value = defaultValue;
    const auto& valueIt = jsonNode.FindMember(key);
    if (valueIt != jsonNode.MemberEnd()) {
        value = valueIt->value.GetString();
    }

    return value;
}

bool AplCoreConnectionManager::getOptionalBool(
        const rapidjson::Value& jsonNode,
        const std::string& key,
        bool defaultValue) {
    bool value = defaultValue;
    const auto& valueIt = jsonNode.FindMember(key);
    if (valueIt != jsonNode.MemberEnd()) {
        value = valueIt->value.GetBool();
    }

    return value;
}

int AplCoreConnectionManager::getOptionalInt(
        const rapidjson::Value& jsonNode,
        const std::string& key,
        int defaultValue) {
    if (jsonNode.HasMember(key) && jsonNode[key].IsInt()) {
        return jsonNode[key].GetInt();
    }

    return defaultValue;
}

bool AplCoreConnectionManager::shouldHandleMessage(const std::string& message) {
    if (m_blockingSendReplyExpected) {
        rapidjson::Document doc;
        if (doc.Parse(message.c_str()).HasParseError()) {
            auto aplOptions = m_aplConfiguration->getAplOptions();
            aplOptions->logMessage(LogLevel::ERROR, "shouldHandleMessageFailed", "Error whilst parsing message");
            return false;
        }

        if (doc.HasMember(SEQNO_KEY) && doc[SEQNO_KEY].IsNumber()) {
            unsigned int seqno = doc[SEQNO_KEY].GetUint();
            if (seqno == m_replyExpectedSequenceNumber) {
                m_blockingSendReplyExpected = false;
                m_replyPromise.set_value(message);
                return false;
            }
        }
    }

    return true;
}

void AplCoreConnectionManager::handleMessage(const std::string& message) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    rapidjson::Document doc;
    if (doc.Parse(message.c_str()).HasParseError()) {
        aplOptions->logMessage(LogLevel::ERROR, "handleMessageFailed", "Error whilst parsing message");
        return;
    }

    if (!doc.HasMember("type")) {
        aplOptions->logMessage(LogLevel::ERROR, "handleMessageFailed", "Unable to find type in message");
        return;
    }
    std::string type = doc["type"].GetString();

    auto payload = doc.FindMember("payload");
    if (payload == doc.MemberEnd()) {
        aplOptions->logMessage(LogLevel::ERROR, "handleMessageFailed", "Unable to find payload in message");
        return;
    }

    auto fit = m_messageHandlers.find(type);
    if (fit != m_messageHandlers.end()) {
        fit->second(payload->value);
    } else {
        aplOptions->logMessage(LogLevel::ERROR, "handleMessageFailed", "Unrecognized message type: " + type);
    }
}

void AplCoreConnectionManager::handleConfigurationChange(const rapidjson::Value& configurationChange) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root || !m_AplCoreMetrics) {
        aplOptions->logMessage(LogLevel::ERROR, "handleConfigurationChangeFailed", "Root context is missing");
        return;
    }

    apl::ConfigurationChange configChange = apl::ConfigurationChange();
    // config change for width and height
    if (configurationChange.HasMember(WIDTH_KEY) && configurationChange[WIDTH_KEY].IsInt() && configurationChange.HasMember(HEIGHT_KEY) && configurationChange[HEIGHT_KEY].IsInt()) {

        m_Metrics.size((int) configurationChange[WIDTH_KEY].GetInt(), (int) configurationChange[HEIGHT_KEY].GetInt());
        m_AplCoreMetrics.reset();
        apl::ScalingOptions scalingOptions = {
                m_ViewportSizeSpecifications, SCALING_BIAS_CONSTANT, SCALING_SHAPE_OVERRIDES_COST};
        if (!scalingOptions.getSpecifications().empty()) {
            m_AplCoreMetrics = std::make_shared<AplCoreMetrics>(m_Metrics, scalingOptions);
        } else {
            m_AplCoreMetrics = std::make_shared<AplCoreMetrics>(m_Metrics);
        }

        const int pixelWidth = (int) m_AplCoreMetrics->toCorePixel(m_AplCoreMetrics->getViewhostWidth());
        const int pixelHeight = (int) m_AplCoreMetrics->toCorePixel(m_AplCoreMetrics->getViewhostHeight());
        configChange = configChange.size(pixelWidth, pixelHeight);
        sendViewhostScalingMessage();
    }
    // config change for theme
    if (configurationChange.HasMember(DOCTHEME_KEY) && configurationChange[DOCTHEME_KEY].IsString()) {
        configChange = configChange.theme(configurationChange[DOCTHEME_KEY].GetString());
    }
    // config change for mode
    if (configurationChange.HasMember(MODE_KEY) &&
            configurationChange[MODE_KEY].IsString() &&
        AVS_VIEWPORT_MODE_MAP.find(configurationChange[MODE_KEY].GetString()) != AVS_VIEWPORT_MODE_MAP.end()) {
        configChange = configChange.mode(AVS_VIEWPORT_MODE_MAP.at(configurationChange[MODE_KEY].GetString()));
    }
    // config change for fontScale
    if (configurationChange.HasMember(FONTSCALE_KEY) && configurationChange[FONTSCALE_KEY].IsFloat()) {
        configChange = configChange.fontScale(configurationChange[FONTSCALE_KEY].GetFloat());
    }
    // config change for screenMode
    if (configurationChange.HasMember(SCREENMODE_KEY) &&
            configurationChange[SCREENMODE_KEY].IsString() &&
        AVS_SCREEN_MODE_MAP.find(configurationChange[SCREENMODE_KEY].GetString()) != AVS_SCREEN_MODE_MAP.end()) {
        configChange = configChange.screenMode(AVS_SCREEN_MODE_MAP.at(configurationChange[SCREENMODE_KEY].GetString()));
    }
    // config change for screenReader
    if (configurationChange.HasMember(SCREENREADER_KEY) && configurationChange[SCREENREADER_KEY].IsBool()) {
        configChange = configChange.screenReader(configurationChange[SCREENREADER_KEY].GetBool());
    }
    // config change for disallowVideo
    if (configurationChange.HasMember(DISALLOWVIDEO_KEY) && configurationChange[DISALLOWVIDEO_KEY].IsBool()) {
        configChange = configChange.disallowVideo(configurationChange[DISALLOWVIDEO_KEY].GetBool());
    }

    // config change for environment value
    if (configurationChange.HasMember(ENVIRONMENT_VALUE_KEY) && configurationChange[ENVIRONMENT_VALUE_KEY].IsObject()) {
        for (rapidjson::Value::ConstMemberIterator iter = configurationChange[ENVIRONMENT_VALUE_KEY].MemberBegin(); iter != configurationChange[ENVIRONMENT_VALUE_KEY].MemberEnd(); ++iter){
            configChange = configChange.environmentValue(iter->name.GetString(), iter->value);
        }
    }
    updateConfigurationChange(configChange);
    m_Root->configurationChange(configChange);
}

void AplCoreConnectionManager::handleUpdateDisplayState(const rapidjson::Value& displayState) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleUpdateDisplayStateFailed", "Root context is missing");
        return;
    }

    /// Display State Mapping
    std::map<int, apl::DisplayState> displayStateMapping = {
        {0, apl::DisplayState::kDisplayStateHidden},
        {1, apl::DisplayState::kDisplayStateBackground},
        {2, apl::DisplayState::kDisplayStateForeground}
    };

    int state = displayState.GetInt();

    if (displayStateMapping.find(state) == displayStateMapping.end()) {
        aplOptions->logMessage(LogLevel::ERROR, "handleUpdateDisplayStateFailed", "Valid state not found");
        return;
    } else {
        m_Root->updateDisplayState(displayStateMapping[state]);
    }
}

void AplCoreConnectionManager::executeCommands(const std::string& command, const std::string& token) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    auto commandFailedFunc = [this, token](const std::string& failureMessage) {
        auto aplOptions = m_aplConfiguration->getAplOptions();
        aplOptions->logMessage(LogLevel::ERROR, "executeCommandsFailed", failureMessage);
        aplOptions->onCommandExecutionComplete(token, AplCommandExecutionEvent::FAILED, failureMessage);
    };

    if (!m_Root) {
        commandFailedFunc("Root context is missing");
        return;
    }

    std::shared_ptr<rapidjson::Document> document(new rapidjson::Document);
    if (document->Parse(command).HasParseError()) {
        commandFailedFunc("Parse commands failed");
        return;
    }

    auto it = document->FindMember("commands");
    if (it == document->MemberEnd() || it->value.GetType() != rapidjson::Type::kArrayType) {
        commandFailedFunc("Missing commands, or is not array");
        return;
    }

    apl::Object object{it->value};
    auto action = m_Root->executeCommands(object, false);
    if (!action) {
        commandFailedFunc("APL Core could not process commands");
        return;
    }

    aplOptions->onActivityStarted(token, APL_COMMAND_EXECUTION);

    auto actionResolvedFunc = [this, document, token](const apl::ActionPtr&) {
        auto aplOptions = m_aplConfiguration->getAplOptions();
        aplOptions->logMessage(LogLevel::DBG, "executeCommandsResolved", "Command sequence completed");
        aplOptions->onCommandExecutionComplete(token, AplCommandExecutionEvent::RESOLVED, "Command sequence completed");
        aplOptions->onActivityEnded(token, APL_COMMAND_EXECUTION);
    };

    auto actionTerminatedFunc = [this, document, token](const apl::TimersPtr&) {
        auto aplOptions = m_aplConfiguration->getAplOptions();
        aplOptions->logMessage(LogLevel::DBG, "executeCommandsTerminated", "Command sequence terminated");
        aplOptions->onCommandExecutionComplete(token, AplCommandExecutionEvent::TERMINATED, "Command sequence terminated");
        aplOptions->onActivityEnded(token, APL_COMMAND_EXECUTION);
    };

    if (action->isPending()) {
        action->then(actionResolvedFunc);
        action->addTerminateCallback(actionTerminatedFunc);
    } else if(action->isResolved()) {
        actionResolvedFunc(nullptr);
    } else if (action->isTerminated()) {
        actionTerminatedFunc(nullptr);
    }
}

void AplCoreConnectionManager::onExtensionEvent(
        const std::string& uri,
        const std::string& name,
        const std::string& source,
        const std::string& params,
        unsigned int event,
        std::shared_ptr<AplCoreExtensionEventCallbackResultInterface> resultCallback) {
    rapidjson::Document sourceDoc;
    rapidjson::Document paramsDoc;

    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (sourceDoc.Parse(source).HasParseError()) {
        aplOptions->logMessage(LogLevel::ERROR, "onExtensionEventFailed", "Parse source failed");
        return;
    }

    if (paramsDoc.Parse(params).HasParseError()) {
        aplOptions->logMessage(LogLevel::ERROR, "onExtensionEventFailed", "Parse params failed");
        return;
    }

    m_extensionManager->onExtensionEvent(
            uri, name, apl::Object(sourceDoc), apl::Object(paramsDoc), event, resultCallback);
}

void AplCoreConnectionManager::onExtensionEventResult(unsigned int event, bool succeeded) {
    rapidjson::Document response(rapidjson::kObjectType);
    rapidjson::Document::AllocatorType& allocator = response.GetAllocator();
    rapidjson::Value payload(rapidjson::kObjectType);
    payload.AddMember(EVENT_KEY, event, allocator);
    payload.AddMember(ARGUMENT_KEY, succeeded ? 0 : 1, allocator);
    handleEventResponse(payload);
}

AplDocumentStatePtr AplCoreConnectionManager::getActiveDocumentState() {
    // If we have active content, report it as an AplDocumentState
    if (m_Content && m_Root && m_AplCoreMetrics) {
        auto documentState = std::make_shared<AplDocumentState>(m_aplToken, m_Root, m_AplCoreMetrics);
        return documentState;
    }
    return nullptr;
}

void AplCoreConnectionManager::restoreDocumentState(AplDocumentStatePtr documentState) {
    m_documentStateToRestore = std::move(documentState);
    m_documentStateToRestore->configurationChange = m_ConfigurationChange;
    reset();
    m_aplConfiguration->getAplOptions()->resetViewhost(m_documentStateToRestore->token);
}

void AplCoreConnectionManager::invokeExtensionEventHandler(
        const std::string& uri,
        const std::string& name,
        const apl::ObjectMap& data,
        bool fastMode) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "invokeExtensionEventHandlerFailed", "Root context is missing");
        return;
    }
    aplOptions->logMessage(LogLevel::DBG, "invokeExtensionEventHandler", +"< " + uri + ":" + name + " >");
    auto actnPtr = m_Root->invokeExtensionEventHandler(uri, name, data, fastMode);
    if (actnPtr == nullptr) {
        aplOptions->logMessage(LogLevel::ERROR, "invokeExtensionEventHandlerFailed", "No handler found");
    }
}

void AplCoreConnectionManager::sendExtensionEvent(
        const std::string &uri,
        const std::string &name,
        const apl::Object &source,
        const apl::Object &params,
        unsigned int event,
        std::shared_ptr<AplCoreExtensionEventCallbackResultInterface> resultCallback) {
    auto message = AplCoreViewhostMessage(EXTENSION_MESSAGE_KEY);

    auto &mAlloc = message.alloc();
    rapidjson::Value payload(rapidjson::kObjectType);
    payload.AddMember("URI", uri, mAlloc);
    payload.AddMember("type", "event", mAlloc);
    payload.AddMember("name", name, mAlloc);

    rapidjson::Value jsonParams = params.serialize(mAlloc);
    payload.AddMember("params", jsonParams, mAlloc);

    message.setPayload(std::move(payload));
    send(message);
}

void AplCoreConnectionManager::dataSourceUpdate(
        const std::string& sourceType,
        const std::string& jsonPayload,
        const std::string& token) {

    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "dataSourceUpdateFailed", "Root context is missing");
        return;
    }

    auto provider = m_Root->getRootConfig().getDataSourceProvider(sourceType);
    if (!provider) {
        aplOptions->logMessage(LogLevel::ERROR, "dataSourceUpdateFailed", "Unknown provider requested.");
        return;
    }

    bool result = provider->processUpdate(jsonPayload);
    if (!result) {
        aplOptions->logMessage(LogLevel::ERROR, "dataSourceUpdateFailed", "Update is not processed.");
        checkAndSendDataSourceErrors();
    }
}

void AplCoreConnectionManager::provideState(unsigned int stateRequestToken) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    auto timer = m_aplConfiguration->getMetricsRecorder()->createTimer(
            Telemetry::AplMetricsRecorderInterface::CURRENT_DOCUMENT,
            "APL-Web.RootContext.notifyVisualContext");
    timer->start();
    rapidjson::Document state(rapidjson::kObjectType);
    rapidjson::Document::AllocatorType& allocator = state.GetAllocator();
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    // Add presentation token info
    state.AddMember(TOKEN_KEY, m_aplToken, allocator);

    // Add version info
    state.AddMember(VERSION_KEY, CLIENT_VERSION_PREFIX + apl::APLVersion::getDefaultReportedVersionString(), allocator);

    // Add visual context info
    state.AddMember(VISUAL_CONTEXT_KEY, getVisualContext(allocator), allocator);

    // Add datasource context info
    state.AddMember(DATASOURCE_CONTEXT_KEY, getDataSourceContext(allocator), allocator);

    state.Accept(writer);
    aplOptions->onVisualContextAvailable(m_aplToken, stateRequestToken, buffer.GetString());
    timer->stop();
}

rapidjson::Value AplCoreConnectionManager::getVisualContext(rapidjson::Document::AllocatorType& allocator) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    auto timer = m_aplConfiguration->getMetricsRecorder()->createTimer(
            Telemetry::AplMetricsRecorderInterface::CURRENT_DOCUMENT,
            "APL-Web.RootContext.serializeVisualContext");
    timer->start();
    rapidjson::Value arr(rapidjson::kArrayType);
    if (m_Root) {
        auto context = m_Root->serializeVisualContext(allocator);
        arr.PushBack(context, allocator);
    } else {
        aplOptions->logMessage(LogLevel::ERROR, "getVisualContextFailed", "Unable to get visual context");
        rapidjson::Value emptyObj(rapidjson::kObjectType);
        // add an empty visual context
        arr.PushBack(emptyObj, allocator);
    }
    timer->stop();
    return arr;
}

void AplCoreConnectionManager::getVisualContext(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "getVisualContextFailed", "Unable to get visual context");
        return;
    }
    auto messageId = payload["messageId"].GetString();
    auto message = AplCoreViewhostMessage("getVisualContext");
    auto& alloc = message.alloc();
    auto result = m_Root->serializeVisualContext(alloc);

    rapidjson::Value outPayload(rapidjson::kObjectType);
    rapidjson::Value value;
    value.SetString(messageId, alloc);
    outPayload.AddMember("messageId", value, alloc);
    outPayload.AddMember("result", result, alloc);
    message.setPayload(std::move(outPayload));
    send(message);
}

rapidjson::Value AplCoreConnectionManager::getDataSourceContext(rapidjson::Document::AllocatorType& allocator) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    auto timer = m_aplConfiguration->getMetricsRecorder()->createTimer(
            Telemetry::AplMetricsRecorderInterface::CURRENT_DOCUMENT,
            "APL-Web.RootContext.serializeDataSourceContext");
    timer->start();
    rapidjson::Value context;

    if (m_Root) {
        context = m_Root->serializeDataSourceContext(allocator);
    } else {
        aplOptions->logMessage(LogLevel::ERROR, "getDataSourceContextFailed", "Unable to get datasource context");
        rapidjson::Value emptyDataSource(rapidjson::kArrayType);
        // return empty datasource context
        context = emptyDataSource;
    }
    timer->stop();
    return context;
}

void AplCoreConnectionManager::getDataSourceContext(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "getDataSourceContextFailed", "Unable to get datasource context");
        return;
    }
    auto messageId = payload["messageId"].GetString();
    auto message = AplCoreViewhostMessage("getDataSourceContext");
    auto& alloc = message.alloc();
    auto result = m_Root->serializeDataSourceContext(alloc);

    rapidjson::Value outPayload(rapidjson::kObjectType);
    rapidjson::Value value;
    value.SetString(messageId, alloc);
    outPayload.AddMember("messageId", value, alloc);
    outPayload.AddMember("result", result, alloc);
    message.setPayload(std::move(outPayload));
    send(message);
}

void AplCoreConnectionManager::interruptCommandSequence() {
    if (m_Root) {
        m_Root->cancelExecution();
    }
}

void AplCoreConnectionManager::updateViewhostConfig(const AplViewhostConfigPtr viewhostConfig) {
    m_viewhostConfig = viewhostConfig;

    m_RootConfig.set({
                {apl::RootProperty::kAgentName, viewhostConfig->agentName()},
                {apl::RootProperty::kAgentVersion, viewhostConfig->agentVersion()},
                {apl::RootProperty::kAllowOpenUrl, viewhostConfig->allowOpenUrl()},
                {apl::RootProperty::kDisallowVideo, viewhostConfig->disallowVideo()},
                {apl::RootProperty::kScrollCommandDuration, viewhostConfig->scrollCommandDuration()},
                {apl::RootProperty::kDisallowEditText, viewhostConfig->disallowEditText()},
                {apl::RootProperty::kDisallowDialog, viewhostConfig->disallowDialog()},
                {apl::RootProperty::kAnimationQuality, static_cast<apl::RootConfig::AnimationQuality>(viewhostConfig->animationQuality())}
            });

    m_Metrics.size(viewhostConfig->viewportWidth(), viewhostConfig->viewportHeight())
            .dpi(viewhostConfig->viewportDpi())
            .shape(viewhostConfig->viewportShape())
            .mode(viewhostConfig->viewportMode());
}

bool
AplCoreConnectionManager::loadPackage(const apl::ContentPtr& content) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    auto metricsRecorder = m_aplConfiguration->getMetricsRecorder();

    auto cImports = metricsRecorder->createCounter(
            Telemetry::AplMetricsRecorderInterface::LATEST_DOCUMENT,
            "APL-Web.Content.imports");
    auto cError = metricsRecorder->createCounter(
            Telemetry::AplMetricsRecorderInterface::LATEST_DOCUMENT,
            "APL-Web.Content.error");

    std::unordered_map<uint32_t, std::future<std::string>> packageContentByRequestId;
    std::unordered_map<uint32_t, apl::ImportRequest> packageRequestByRequestId;
    while (content->isWaiting() && !content->isError()) {
        auto packages = content->getRequestedPackages();
        cImports->incrementBy(packages.size());
        unsigned int count = 0;
        for (auto& package : packages) {
            auto name = package.reference().name();
            auto version = package.reference().version();
            auto source = package.source();

            aplOptions->logMessage(
                    LogLevel::DBG, "loadPackage", "Requesting package: " + name + " " + version);

            if (source.empty()) {
                char sourceBuffer[CHUNK_SIZE];
                snprintf(sourceBuffer, CHUNK_SIZE, ALEXA_IMPORT_PATH, name.c_str(), version.c_str());
                source = sourceBuffer;
            }

            auto packageContentFuture =
                async(std::launch::async, &AplOptionsInterface::downloadResource, aplOptions, source);
            packageContentByRequestId.insert(std::make_pair(package.getUniqueId(), std::move(packageContentFuture)));
            packageRequestByRequestId.insert(std::make_pair(package.getUniqueId(), package));
            count++;

            // if we reach the maximum number of concurrent downloads or already go through all packages, wait for them
            // to finish
            if (count % aplOptions->getMaxNumberOfConcurrentDownloads() == 0 || packages.size() == count) {
                for (auto& kvp : packageContentByRequestId) {
                    auto packageContent = kvp.second.get();
                    if (packageContent.empty()) {
                        aplOptions->logMessage(
                            LogLevel::ERROR, "renderByAplCoreFailed", "Could not be retrieve requested import");
                        return false;
                    }
                    content->addPackage(packageRequestByRequestId.at(kvp.first), packageContent);
                }
                packageContentByRequestId.clear();
                packageRequestByRequestId.clear();
            }
        }
    }

    return !content->isError();
}

bool AplCoreConnectionManager::registerRequestedExtensions() {
    // Extensions requested by the content
    auto requestedExtensions = m_Content->getExtensionRequests();

    if (m_extensionManager->useAlexaExt()) {
        if (!initAlexaExts(requestedExtensions)) {
            // Required extensions have not loaded.
            return false;
        }
    } else {
        initLegacyExts(requestedExtensions);
    }

    return true;
}

void AplCoreConnectionManager::handleBuild(const rapidjson::Value& message) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    auto inflationTimer = m_aplConfiguration->getMetricsRecorder()->createTimer(
            Telemetry::AplMetricsRecorderInterface::LATEST_DOCUMENT,
            Telemetry::AplRenderingSegment::kRootContextInflation);
    inflationTimer->start();

    /* APL Document Inflation started */
    aplOptions->onRenderingEvent(m_aplToken, AplRenderingEvent::INFLATE_BEGIN);

    if (m_documentStateToRestore) {
        // Restore from document state
        m_aplToken = m_documentStateToRestore->token;
        m_Root = m_documentStateToRestore->rootContext;
        m_Content = m_Root->content();
        m_RootConfig = m_Root->getRootConfig();
        coreFrameUpdate();
        m_Root->configurationChange(m_documentStateToRestore->configurationChange);
    }

    if (!m_Content) {
        aplOptions->logMessage(LogLevel::WARN, "handleBuildFailed", "No content to build");
        sendError("No content to build");
        inflationTimer->fail();
        return;
    }

    // Get APL Version for content
    std::string aplVersion = m_Content->getAPLVersion();

    if (!m_audioPlayerFactory) {
        m_audioPlayerFactory = AplCoreAudioPlayerFactory::create(shared_from_this(), m_aplConfiguration);
    }

    if (!m_mediaPlayerFactory) {
        m_mediaPlayerFactory = AplCoreMediaPlayerFactory::create(shared_from_this(), m_aplConfiguration);
    }

    // If we're not restoring a document state, create a new RootConfig.
    if (!m_documentStateToRestore) {
        std::string agentName = getOptionalValue(message, AGENTNAME_KEY, "wssHost");
        std::string agentVersion = getOptionalValue(message, AGENTVERSION_KEY, "1.0");
        bool allowOpenUrl = getOptionalBool(message, ALLOWOPENURL_KEY, false);
        bool disallowVideo = getOptionalBool(message, DISALLOWVIDEO_KEY, false);
        bool disallowDialog = getOptionalBool(message, DISALLOWDIALOG_KEY, false);
        bool disallowEditText = getOptionalBool(message, DISALLOWEDITTEXT_KEY, false);
        int scrollCommandDuration = (int)getOptionalValue(message, SCROLL_COMMAND_DURATION_KEY, 1000);
        int animationQuality =
            getOptionalInt(message, ANIMATIONQUALITY_KEY, apl::RootConfig::AnimationQuality::kAnimationQualityNormal);

        m_RootConfig = apl::RootConfig().set({
                         {apl::RootProperty::kAgentName, agentName},
                         {apl::RootProperty::kAgentVersion, agentVersion},
                         {apl::RootProperty::kAllowOpenUrl, allowOpenUrl},
                         {apl::RootProperty::kDisallowVideo, disallowVideo},
                         {apl::RootProperty::kScrollCommandDuration, scrollCommandDuration},
                         {apl::RootProperty::kDisallowEditText, disallowEditText},
                         {apl::RootProperty::kDisallowDialog, disallowDialog},
                         {apl::RootProperty::kAnimationQuality, static_cast<apl::RootConfig::AnimationQuality>(animationQuality)},
                         {apl::RootProperty::kUTCTime, getCurrentTime().count()},
                         {apl::RootProperty::kLocalTimeAdjustment, aplOptions->getTimezoneOffset().count()},
                         {apl::RootProperty::kDefaultIdleTimeout, -1},
                         {apl::RootProperty::kDefaultFontFamily, DEFAULT_FONT}
                      })
                     .measure(std::make_shared<AplCoreTextMeasurement>(shared_from_this(), m_aplConfiguration))
                     .localeMethods(std::make_shared<AplCoreLocaleMethods>(shared_from_this(), m_aplConfiguration))
                     .enforceAPLVersion(apl::APLVersion::kAPLVersionIgnore)
                     .enableExperimentalFeature(apl::RootConfig::ExperimentalFeature::kExperimentalFeatureManageMediaRequests)
                     .audioPlayerFactory(m_audioPlayerFactory)
                     .mediaPlayerFactory(m_mediaPlayerFactory);

        // Data Sources
        m_RootConfig.dataSourceProvider(
            apl::DynamicIndexListConstants::DEFAULT_TYPE_NAME,
            std::make_shared<apl::DynamicIndexListDataSourceProvider>());

        m_RootConfig.dataSourceProvider(
                apl::DynamicTokenListConstants::DEFAULT_TYPE_NAME,
                std::make_shared<apl::DynamicTokenListDataSourceProvider>());

        // Handle metrics data
        m_Metrics.size(message[WIDTH_KEY].GetInt(), message[HEIGHT_KEY].GetInt())
            .dpi(message[DPI_KEY].GetInt())
            .shape(AVS_VIEWPORT_SHAPE_MAP.at(message[SHAPE_KEY].GetString()))
            .mode(AVS_VIEWPORT_MODE_MAP.at(message[MODE_KEY].GetString()));

        m_Content->refresh(m_Metrics, m_RootConfig);
        if(!loadPackage(m_Content)) {
            aplOptions->logMessage(
                    LogLevel::WARN, __func__, "Unable to refresh content");
            sendError("Content failed to prepare");
            inflationTimer->fail();
            return;
        }
    }

    // Extension initialisation
    m_supportedExtensions.clear();
    if (message.HasMember(SUPPORTED_EXTENSIONS) && message[SUPPORTED_EXTENSIONS].IsArray()) {
        // Extensions supported by the client renderer instance
        auto supportedExtensions = message[SUPPORTED_EXTENSIONS].GetArray();

        for (auto& ext : supportedExtensions) {
            auto supportedExtension = std::make_shared<SupportedExtension>();
            if (ext.IsString()) {
                // No flags provided for extension initialisation
                supportedExtension->uri = ext.GetString();
            } else {
                // Flags can be supplied via SUPPORTED_EXTENSIONS using SupportedExtension::APLWSRenderer.d.ts
                // e.g. `supportedExtensions.push({uri: 'aplext:e2eencryption:10', flags: 'aFlag' })`
                //
                // Allowed formats:
                // - an unkeyed container (array)
                // - a key-value bag (keyed container)
                // - a single non-null value
                if (ext.IsObject() && ext.HasMember(URI_KEY) && ext[URI_KEY].IsString()) {
                    supportedExtension->uri = ext[URI_KEY].GetString();

                    // Have optional flags been provided?
                    if (ext.HasMember(FLAGS_KEY)) {
                        if (ext[FLAGS_KEY].IsArray() || ext[FLAGS_KEY].IsObject() || ext[FLAGS_KEY].IsString()) {
                            supportedExtension->flags = ext[FLAGS_KEY];
                        } else {
                            aplOptions->logMessage(LogLevel::WARN, "handleBuildFailed", "SUPPORTED_EXTENSIONS flags entry not formatted correctly");
                        }
                    }
                } else {
                    aplOptions->logMessage(LogLevel::WARN, "handleBuildFailed", "SUPPORTED_EXTENSIONS entry not formatted correctly");
                    continue;
                }
            }
            m_supportedExtensions.push_back(supportedExtension);
        }
    }

    if (!registerRequestedExtensions()) {
        // If using AlexaExt: Required extensions have not loaded
        aplOptions->logMessage(LogLevel::ERROR, "handleBuildFailed", "Required extensions have not loaded");
        sendError("Required extensions have not loaded");

        inflationTimer->stop();
        return;
    }

    auto renderingOptionsMsg = AplCoreViewhostMessage(RENDERING_OPTIONS_KEY);
    rapidjson::Value renderingOptions(rapidjson::kObjectType);
    renderingOptions.AddMember(LEGACY_KARAOKE_KEY, aplVersion == "1.0", renderingOptionsMsg.alloc());
    renderingOptions.AddMember(DOCUMENT_APL_VERSION_KEY, aplVersion, renderingOptionsMsg.alloc());
    send(renderingOptionsMsg.setPayload(std::move(renderingOptions)));

    m_PendingEvents.clear();

    // Release the activity tracker
    aplOptions->onActivityEnded(m_aplToken, APL_COMMAND_EXECUTION);

    if (m_ScreenLock) {
        aplOptions->onActivityEnded(m_aplToken, APL_SCREEN_LOCK);
        m_ScreenLock = false;
    }

    m_StartTime = getCurrentTime();

    // If we're not restoring a document state, then create metrics and RootContext
    if (!m_documentStateToRestore) {
        do {
            apl::ScalingOptions scalingOptions = {
                m_ViewportSizeSpecifications, SCALING_BIAS_CONSTANT, SCALING_SHAPE_OVERRIDES_COST};
            if (!scalingOptions.getSpecifications().empty()) {
                m_AplCoreMetrics = std::make_shared<AplCoreMetrics>(m_Metrics, scalingOptions);
            } else {
                m_AplCoreMetrics = std::make_shared<AplCoreMetrics>(m_Metrics);
            }

            sendViewhostScalingMessage();

            m_StartTime = getCurrentTime();
            m_Root = apl::RootContext::create(m_AplCoreMetrics->getMetrics(), m_Content, m_RootConfig);
            if (m_Root) {
                break;
            } else if (!m_ViewportSizeSpecifications.empty()) {
                aplOptions->logMessage(
                    LogLevel::WARN, __func__, "Unable to inflate document with current chosen scaling.");
            }

            auto it = m_ViewportSizeSpecifications.begin();
            for (; it != m_ViewportSizeSpecifications.end(); it++) {
                if (*it == m_AplCoreMetrics->getChosenSpec()) {
                    m_ViewportSizeSpecifications.erase(it);
                    break;
                }
            }
            if (it == m_ViewportSizeSpecifications.end()) {
                // Core returned specification that is not in list. Something went wrong. Prevent infinite loop.
                break;
            }
        } while (!m_ViewportSizeSpecifications.empty());
    }

    // Make sure we only restore a documentState once.
    if (m_documentStateToRestore) {
        m_documentStateToRestore.reset();
    }

    // Get background
    apl::Object background = m_Content->getBackground(m_AplCoreMetrics->getMetrics(), m_Root->getRootConfig());

    bool supportsResizing = false;

    // Get Document Settings
    if (auto documentSettings = m_Content->getDocumentSettings()) {
        // Get resizing setting
        supportsResizing = documentSettings->getValue(SUPPORTS_RESIZING_KEY).asBoolean();
    }

    sendSupportsResizingMessage(supportsResizing);

    /* APL Core Inflation ended */
    aplOptions->onRenderingEvent(m_aplToken, AplRenderingEvent::INFLATE_END);

    if (m_Root) {
        inflationTimer->stop();
        // Init viewhost globals
        sendViewhostScalingMessage();
        sendDocumentBackgroundMessage(background);

        // Start rendering component hierarchy
        // and displaying Children
        sendHierarchy(HIERARCHY_KEY);

        auto idleTimeout = std::chrono::milliseconds(m_Content->getDocumentSettings()->idleTimeout(m_Root->getRootConfig()));
        aplOptions->onSetDocumentIdleTimeout(m_aplToken, idleTimeout);
        aplOptions->onRenderDocumentComplete(m_aplToken, true, "");
    } else {
        inflationTimer->fail();
        aplOptions->logMessage(LogLevel::ERROR, "handleBuildFailed", "Unable to inflate document");
        sendError("Unable to inflate document");
        aplOptions->onRenderDocumentComplete(m_aplToken, false, "Unable to inflate document");
        // Send DataSource errors if any
        checkAndSendDataSourceErrors();
    }
}

bool AplCoreConnectionManager::initAlexaExts(const std::set<std::string>& requestedExtensions) {
    auto extensionMediator = apl::ExtensionMediator::create(
        m_extensionManager->getExtensionRegistrar(),
        m_extensionManager->getExtensionExecutor()
    );

    m_RootConfig.enableExperimentalFeature(apl::RootConfig::ExperimentalFeature::kExperimentalFeatureExtensionProvider)
        .extensionProvider(m_extensionManager->getExtensionRegistrar())
        .extensionMediator(extensionMediator);

    std::condition_variable notifyLoaded;
    std::mutex extensionMutex;
    const std::chrono::milliseconds maxWaitTime = std::chrono::milliseconds(5000);

    // Extension Granting
    std::set<std::string> grantedURIs;
    apl::ObjectMap flagMap;

    for (auto& ext : m_supportedExtensions) {
        if (!ext->flags.empty()) {
            flagMap[ext->uri] = ext->flags;
        }
        
        if (requestedExtensions.find(ext->uri) != requestedExtensions.end()) {
            if (auto extension = m_extensionManager->getAlexaExtExtension(ext->uri)) {
                grantedURIs.emplace(ext->uri);
            }
        }
    }

    extensionMediator->initializeExtensions(
        flagMap,
        m_Content,
        [grantedURIs](const std::string& uri,
            apl::ExtensionMediator::ExtensionGrantResult grant,
            apl::ExtensionMediator::ExtensionGrantResult deny) {
                grantedURIs.count(uri) > 0 ? grant(uri) : deny (uri);
            }
    );

    auto loadingFinished = false;
    auto loadingFailed = false;
    extensionMediator->loadExtensions(
        flagMap,
        m_Content,
        [&](bool success){
            // ExtensionLoadedCallback 
            std::lock_guard<std::mutex> lock(extensionMutex);
            loadingFinished = true;
            loadingFailed = !success;
            notifyLoaded.notify_all();
        }
    );

    {
        std::unique_lock<std::mutex> waitLock(extensionMutex);
        auto res = notifyLoaded.wait_for(waitLock, maxWaitTime, [&]() {
            return loadingFinished;
        });

        if (!res) {
            m_aplConfiguration->getAplOptions()->logMessage(LogLevel::ERROR, "initAlexaExtsFailed", "Timed out waiting for extensions to load. Some extensions may not be loaded.");
            sendError("Timed out waiting for extensions to load. Some extensions may not be loaded.");
        }
        if (loadingFailed) {
            m_aplConfiguration->getAplOptions()->logMessage(LogLevel::ERROR, "initAlexaExtsFailed", "Required extension loading failed.");
            sendError("Required extension loading failed.");
        }
    }
    return !loadingFailed;
}

void AplCoreConnectionManager::initLegacyExts(const std::set<std::string>& requestedExtensions) {
    for (auto& ext: m_supportedExtensions) {
        // If the supported extension is both requested and available, register it with the config
        if (requestedExtensions.find(ext->uri) != requestedExtensions.end()) {
            if (auto extension = m_extensionManager->getExtension(ext->uri)) {
                // Apply content defined settings to extension
                auto extSettings = m_Content->getExtensionSettings(ext->uri);
                extension->applySettings(extSettings);
                m_extensionManager->registerRequestedExtension(extension->getUri(), m_RootConfig);
            }
        }
    }
}

void AplCoreConnectionManager::onDocumentRendered(
        const std::chrono::steady_clock::time_point &renderTime,
        uint64_t complexityScore) {
    auto metricsRecorder = m_aplConfiguration->getMetricsRecorder();
    metricsRecorder->flush();
}

void AplCoreConnectionManager::sendViewhostScalingMessage() {
    if (m_AplCoreMetrics) {
        // Send scaling metrics out to viewhost
        auto reply = AplCoreViewhostMessage(SCALING_KEY);
        rapidjson::Value scaling(rapidjson::kObjectType);
        scaling.AddMember(SCALE_FACTOR_KEY, m_AplCoreMetrics->toViewhost(1.0f), reply.alloc());
        scaling.AddMember(VIEWPORT_WIDTH_KEY, m_AplCoreMetrics->getViewhostWidth(), reply.alloc());
        scaling.AddMember(VIEWPORT_HEIGHT_KEY, m_AplCoreMetrics->getViewhostHeight(), reply.alloc());
        send(reply.setPayload(std::move(scaling)));
    }
}

void AplCoreConnectionManager::sendDocumentBackgroundMessage(const apl::Object& background) {
    auto backgroundMsg = AplCoreViewhostMessage(BACKGROUND_KEY);
    auto& alloc = backgroundMsg.alloc();
    rapidjson::Value payload(rapidjson::kObjectType);
    rapidjson::Value backgroundValue(rapidjson::kObjectType);
    if (background.is<apl::Color>()) {
        backgroundValue.AddMember(COLOR_KEY, background.asString(), alloc);
    } else if (background.is<apl::Gradient>()) {
        backgroundValue.AddMember(GRADIENT_KEY, background.get<apl::Gradient>().serialize(alloc), alloc);
    } else {
        backgroundValue.AddMember(COLOR_KEY, apl::Color().asString(), alloc);
    }
    payload.AddMember(BACKGROUND_KEY, backgroundValue, alloc);
    backgroundMsg.setPayload(std::move(payload));
    send(backgroundMsg);
}

void AplCoreConnectionManager::sendScreenLockMessage(bool screenLock) {
    auto screenLockMsg = AplCoreViewhostMessage(SCREENLOCK_KEY);
    auto& alloc = screenLockMsg.alloc();
    rapidjson::Value payload(rapidjson::kObjectType);
    payload.AddMember(SCREENLOCK_KEY, screenLock, alloc);
    screenLockMsg.setPayload(std::move(payload));
    send(screenLockMsg);
}

void AplCoreConnectionManager::sendSupportsResizingMessage(bool supportsResizing) {
    auto supportsResizingMsg = AplCoreViewhostMessage(SUPPORTS_RESIZING_KEY);
    auto& alloc = supportsResizingMsg.alloc();
    rapidjson::Value payload(rapidjson::kObjectType);
    payload.AddMember(SUPPORTS_RESIZING_KEY, supportsResizing, alloc);
    supportsResizingMsg.setPayload(std::move(payload));
    send(supportsResizingMsg);
}

void AplCoreConnectionManager::handleUpdate(const rapidjson::Value& update) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleUpdateFailed", "Root context is null");
        return;
    }

    auto id = update["id"].GetString();
    auto component = m_Root->findComponentById(id);
    if (!component) {
        aplOptions->logMessage(
            LogLevel::ERROR, "handleUpdateFailed", std::string("Unable to find component with id: ") + id);
        sendError("Unable to find component");
        return;
    }

    auto type = static_cast<apl::UpdateType>(update["type"].GetInt());

    if (update["value"].IsString()) {
        std::string value = update["value"].GetString();
        component->update(type, value);
    } else {
        auto value = update["value"].GetFloat();
        component->update(type, value);
    }
}

void AplCoreConnectionManager::handleMediaUpdate(const rapidjson::Value& update) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleMediaUpdateFailed", "Root context is null");
        return;
    }

    auto id = update["id"].GetString();
    auto component = m_Root->findComponentById(id);
    if (!component) {
        aplOptions->logMessage(
            LogLevel::ERROR, "handleMediaUpdateFailed", std::string("Unable to find component with id: ") + id);
        sendError("Unable to find component");
        return;
    }

    if (!update.HasMember(MEDIA_STATE_KEY) || !update.HasMember(FROM_EVENT_KEY)) {
        aplOptions->logMessage(
            LogLevel::ERROR, "handleMediaUpdateFailed", "State update object is missing parameters");
        sendError("Can't update media state.");
        return;
    }
    auto& state = update[MEDIA_STATE_KEY];
    auto fromEvent = update[FROM_EVENT_KEY].GetBool();

    if (!state.HasMember(TRACK_INDEX_KEY) || !state.HasMember(TRACK_COUNT_KEY) || !state.HasMember(CURRENT_TIME_KEY) ||
        !state.HasMember(DURATION_KEY) || !state.HasMember(PAUSED_KEY) || !state.HasMember(ENDED_KEY) ||
        !state.HasMember(TRACK_STATE_KEY) || !state.HasMember(MUTED_KEY)) {
        aplOptions->logMessage(
            LogLevel::ERROR, "handleMediaUpdateFailed", "Can't update media state. MediaStatus structure is wrong");
        sendError("Can't update media state.");
        return;
    }

    // numeric parameters are sometimes converted to null during stringification, set these to 0
    const int trackIndex = getOptionalInt(state, TRACK_INDEX_KEY, 0);
    const int trackCount = getOptionalInt(state, TRACK_COUNT_KEY, 0);
    const int currentTime = (int) getOptionalValue(state, CURRENT_TIME_KEY, 0);
    const int duration = (int) getOptionalValue(state, DURATION_KEY, 0);
    const apl::TrackState trackState = static_cast<apl::TrackState>(state[TRACK_STATE_KEY].GetInt());

    apl::MediaState mediaState(
            trackIndex, trackCount, currentTime, duration,
            state[PAUSED_KEY].GetBool(),
            state[ENDED_KEY].GetBool(),
            state[MUTED_KEY].GetBool());

    mediaState.withTrackState(trackState);
    component->updateMediaState(mediaState, fromEvent);
}

void AplCoreConnectionManager::handleGraphicUpdate(const rapidjson::Value& update) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleGraphicUpdateFailed", "Root context is null");
        return;
    }

    auto id = update["id"].GetString();
    auto component = m_Root->findComponentById(id);
    if (!component) {
        aplOptions->logMessage(
            LogLevel::ERROR, "handleGraphicUpdateFailed", std::string("Unable to find component with id:") + id);
        sendError("Unable to find component");
        return;
    }

    auto json = apl::GraphicContent::create(update["avg"].GetString());
    component->updateGraphic(json);
}

void AplCoreConnectionManager::handleEnsureLayout(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleEnsureLayoutFailed", "Root context is null");
        return;
    }

    auto id = payload["id"].GetString();
    auto component = m_Root->findComponentById(id);
    if (!component) {
        aplOptions->logMessage(
            LogLevel::ERROR, "handleEnsureLayoutFailed", std::string("Unable to find component with id:") + id);
        sendError("Unable to find component");
        return;
    }

    auto msg = AplCoreViewhostMessage(ENSURELAYOUT_KEY);
    send(msg.setPayload(id));
}

void AplCoreConnectionManager::handleScrollToRectInComponent(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleScrollToRectInComponentFailed", "Root context is null");
        return;
    }

    auto id = payload["id"].GetString();
    auto component = m_Root->findComponentById(id);
    if (!component) {
        aplOptions->logMessage(
            LogLevel::ERROR,
            "handleScrollToRectInComponentFailed",
            std::string("Unable to find component with id:") + id);
        sendError("Unable to find component");
        return;
    }

    apl::Rect rect = convertJsonToScaledRect(payload);
    m_Root->scrollToRectInComponent(component, rect, static_cast<apl::CommandScrollAlign>(payload["align"].GetInt()));
}

void AplCoreConnectionManager::handleHandleKeyboard(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleHandleKeyboardFailed", "Root context is null");
        return;
    }

    auto messageId = payload["messageId"].GetString();
    auto keyType = payload["keyType"].GetInt();
    auto code = payload["code"].GetString();
    auto key = payload["key"].GetString();
    auto repeat = payload["repeat"].GetBool();
    auto altKey = payload["altKey"].GetBool();
    auto ctrlKey = payload["ctrlKey"].GetBool();
    auto metaKey = payload["metaKey"].GetBool();
    auto shiftKey = payload["shiftKey"].GetBool();
    apl::Keyboard keyboard(code, key);
    keyboard.repeat(repeat);
    keyboard.alt(altKey);
    keyboard.ctrl(ctrlKey);
    keyboard.meta(metaKey);
    keyboard.shift(shiftKey);
    bool result = m_Root->handleKeyboard(static_cast<apl::KeyHandlerType>(keyType), keyboard);

    auto handleKeyboardResultMessage = AplCoreViewhostMessage("handleKeyboard");
    auto& alloc = handleKeyboardResultMessage.alloc();
    rapidjson::Value handleKeyboardResultValue(rapidjson::kObjectType);

    rapidjson::Value value;
    value.SetString(messageId, alloc);
    handleKeyboardResultValue.AddMember("messageId", value, alloc);
    handleKeyboardResultValue.AddMember("result", result, alloc);

    handleKeyboardResultMessage.setPayload(std::move(handleKeyboardResultValue));
    send(handleKeyboardResultMessage);
}

void AplCoreConnectionManager::getFocusableAreas(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "getFocusableAreasFailed", "Root context is null");
        return;
    }

    auto messageId = payload["messageId"].GetString();
    auto message = AplCoreViewhostMessage("getFocusableAreas");
    auto result = m_Root->getFocusableAreas();
    auto& alloc = message.alloc();

    rapidjson::Value outPayload(rapidjson::kObjectType);
    rapidjson::Value value;
    value.SetString(messageId, alloc);
    outPayload.AddMember("messageId", value, alloc);

    rapidjson::Value areas(rapidjson::kObjectType);

    for (auto iterator = result.begin(); iterator != result.end(); iterator++) {
        auto top = iterator->second.getTop();
        auto left = iterator->second.getLeft();
        auto width = iterator->second.getWidth();
        auto height = iterator->second.getHeight();

        rapidjson::Value rectangle(rapidjson::kObjectType);
        rectangle.AddMember("top", top, alloc);
        rectangle.AddMember("left", left, alloc);
        rectangle.AddMember("width", width, alloc);
        rectangle.AddMember("height", height, alloc);
        rapidjson::Value key(rapidjson::kStringType);
        key.SetString(iterator->first.c_str(), alloc);
        areas.AddMember(key, rectangle, alloc);
    }

    outPayload.AddMember("areas", areas, alloc);
    message.setPayload(std::move(outPayload));
    send(message);
}

void AplCoreConnectionManager::getFocused(const rapidjson::Value& payload) {
    auto messageId = payload["messageId"].GetString();
    auto message = AplCoreViewhostMessage("getFocused");

    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "getFocusedFailed", "Root context is null");
        return;
    }

    auto result = m_Root->getFocused();
    auto& alloc = message.alloc();
    rapidjson::Value outPayload(rapidjson::kObjectType);
    rapidjson::Value value;
    value.SetString(messageId, alloc);
    outPayload.AddMember("messageId", value, alloc);
    outPayload.AddMember("result", result, alloc);
    message.setPayload(std::move(outPayload));
    send(message);
}

void AplCoreConnectionManager::setFocus(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "setFocusFailed", "Root context is null");
        return;
    }

    auto direction = payload["direction"].GetInt();
    auto top = payload["origin"].FindMember("top")->value.Get<float>();
    auto left = payload["origin"].FindMember("left")->value.Get<float>();
    auto width = payload["origin"].FindMember("width")->value.Get<float>();
    auto height = payload["origin"].FindMember("height")->value.Get<float>();

    auto origin = apl::Rect(top, left, width, height);
    auto targetId = payload["targetId"].GetString();
    m_Root->setFocus(static_cast<apl::FocusDirection>(direction), origin, targetId);
}

void AplCoreConnectionManager::mediaLoaded(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "AplCoreConnectionManager:mediaLoaded", "Root context is null");
        return;
    }
    auto source = payload["source"].GetString();
    m_Root->mediaLoaded(source);
}

void AplCoreConnectionManager::mediaLoadFailed(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "AplCoreConnectionManager::mediaLoadFailed", "Root context is null");
        return;
    }
    auto source = payload["source"].GetString();
    auto errorCode = payload["errorCode"].GetInt();
    auto error = payload["error"].GetString();
    m_Root->mediaLoadFailed(source, errorCode, error);
}

void AplCoreConnectionManager::audioPlayerCallback(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "AplCoreConnectionManager::audioPlayerCallback", "Root context is null");
        return;
    }

    auto audioFactory = std::dynamic_pointer_cast<AplCoreAudioPlayerFactory>(m_Root->getRootConfig().getAudioPlayerFactory());

    auto playerId = payload["playerId"].GetString();
    auto player = audioFactory->getPlayer(playerId);

    if (player) player->onEvent(payload);
}

void AplCoreConnectionManager::audioPlayerSpeechMarks(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "AplCoreConnectionManager::audioPlayerCallback", "Root context is null");
        return;
    }

    auto audioFactory = std::dynamic_pointer_cast<AplCoreAudioPlayerFactory>(m_Root->getRootConfig().getAudioPlayerFactory());

    auto playerId = payload["playerId"].GetString();
    auto player = audioFactory->getPlayer(playerId);

    if (player) player->onSpeechMarks(payload);
}

void AplCoreConnectionManager::mediaPlayerUpdateMediaState(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "AplCoreConnectionManager::mediaPlayerUpdateMediaState", "Root context is null");
        return;
    }

    auto mediaPlayerFactory = std::dynamic_pointer_cast<AplCoreMediaPlayerFactory>(m_Root->getRootConfig().getMediaPlayerFactory());

    auto playerId = payload["playerId"].GetString();
    auto mediaPlayer = mediaPlayerFactory->getMediaPlayer(playerId);
    if (mediaPlayer) mediaPlayer->updateMediaState(payload);
}

void AplCoreConnectionManager::mediaPlayerDoCallback(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "AplCoreConnectionManager::mediaPlayerDoCallback", "Root context is null");
        return;
    }

    auto mediaPlayerFactory = std::dynamic_pointer_cast<AplCoreMediaPlayerFactory>(m_Root->getRootConfig().getMediaPlayerFactory());

    auto playerId = payload["playerId"].GetString();
    auto mediaPlayer = mediaPlayerFactory->getMediaPlayer(playerId);
    if (mediaPlayer) mediaPlayer->doCallback(payload);
}

void AplCoreConnectionManager::handleUpdateCursorPosition(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleUpdateCursorPositionFailed", "Root context is null");
        return;
    }

    const float x = payload[X_KEY].GetFloat();
    const float y = payload[Y_KEY].GetFloat();
    apl::Point cursorPosition(m_AplCoreMetrics->toCore(x), m_AplCoreMetrics->toCore(y));
    m_Root->handlePointerEvent(apl::PointerEvent(apl::PointerEventType::kPointerMove, cursorPosition));
}

void AplCoreConnectionManager::handleHandlePointerEvent(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleHandlePointerEventFailed", "Root context is null");
        return;
    }

    const float x = payload[X_KEY].GetFloat();
    const float y = payload[Y_KEY].GetFloat();

    auto point = apl::Point(m_AplCoreMetrics->toCore(x), m_AplCoreMetrics->toCore(y));
    auto pointerEventType = static_cast<apl::PointerEventType>(payload[POINTEREVENTTYPE_KEY].GetInt());
    auto pointerType = static_cast<apl::PointerType>(payload[POINTERTYPE_KEY].GetInt());
    auto pointerId = static_cast<apl::id_type>(payload[POINTERID_KEY].GetInt());
    apl::PointerEvent pointerEvent = apl::PointerEvent(pointerEventType, point, pointerId, pointerType);

    m_Root->handlePointerEvent(pointerEvent);
}

void AplCoreConnectionManager::handleEventResponse(const rapidjson::Value& response) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleEventResponseFailed", "Root context is null");
        return;
    }

    if (!response[EVENT_KEY].IsInt()) {
        aplOptions->logMessage(LogLevel::ERROR, "handleEventResponseFailed", "Invalid event response");
        sendError("Invalid event response");
        return;
    }
    auto event = response[EVENT_KEY].GetInt();
    auto it = m_PendingEvents.find(event);
    if (it != m_PendingEvents.end()) {
        auto rectJson = response.FindMember("rectArgument");
        if (rectJson != response.MemberEnd()) {
            apl::Rect rect = convertJsonToScaledRect(rectJson->value);
            it->second.resolve(rect);
        } else {
            auto arg = response.FindMember(ARGUMENT_KEY);
            if (arg != response.MemberEnd()) {
                it->second.resolve(arg->value.GetInt());
            } else {
                it->second.resolve();
            }
        }
        m_PendingEvents.erase(it);
    }
}

unsigned int AplCoreConnectionManager::send(AplCoreViewhostMessage& message) {
    unsigned int seqno = ++m_SequenceNumber;
    m_aplConfiguration->getAplOptions()->sendMessage(m_aplToken, message.setSequenceNumber(seqno).get());
    return seqno;
}

rapidjson::Document AplCoreConnectionManager::blockingSend(
        AplCoreViewhostMessage& message,
        const std::chrono::milliseconds& timeout) {
    std::lock_guard<std::mutex> lock{m_blockingSendMutex};
    m_replyPromise = std::promise<std::string>();
    m_blockingSendReplyExpected = true;
    // Increment expected sequence number first . While send does increment the sequence number, it calls
    // sendMessage before returning the incremented number which creates a race condition in shouldHandleMessage
    m_replyExpectedSequenceNumber = m_SequenceNumber + 1;
    send(message);

    auto aplOptions = m_aplConfiguration->getAplOptions();
    auto future = m_replyPromise.get_future();
    auto status = future.wait_for(timeout);
    if (status != std::future_status::ready) {
        m_blockingSendReplyExpected = false;
        // Under the situation that finish command destroys the renderer, there is no response.
        aplOptions->logMessage(LogLevel::WARN, "blockingSendFailed", "Did not receive response");
        return rapidjson::Document(rapidjson::kNullType);
    }

    rapidjson::Document doc;
    if (doc.Parse(future.get()).HasParseError()) {
        aplOptions->logMessage(LogLevel::ERROR, "blockingSendFailed", "parsingFailed");
        return rapidjson::Document(rapidjson::kNullType);
    }

    return doc;
}

void AplCoreConnectionManager::sendError(const std::string& message) {
    auto reply = AplCoreViewhostMessage(ERROR_KEY);
    send(reply.setPayload(message));
}

void AplCoreConnectionManager::handleScreenLock() {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleScreenLockFailed", "Root context is null");
        return;
    }

    if (m_Root->screenLock() && !m_ScreenLock) {
        aplOptions->onActivityStarted(m_aplToken, APL_SCREEN_LOCK);
        m_ScreenLock = true;
    } else if (!m_Root->screenLock() && m_ScreenLock) {
        aplOptions->onActivityEnded(m_aplToken, APL_SCREEN_LOCK);
        m_ScreenLock = false;
    } else {
        return;
    }
    sendScreenLockMessage(m_ScreenLock);
}

void AplCoreConnectionManager::processEvent(const apl::Event& event) {
    auto aplOptions = m_aplConfiguration->getAplOptions();

    if (apl::EventType::kEventTypeFinish == event.getType()) {
        aplOptions->onFinish(m_aplToken);
        return;
    }

    if (apl::EventType::kEventTypeSendEvent == event.getType()) {
        rapidjson::Document userEventPayloadJson(rapidjson::kObjectType);
        auto& allocator = userEventPayloadJson.GetAllocator();
        auto source = event.getValue(apl::EventProperty::kEventPropertySource);
        auto components = event.getValue(apl::EventProperty::kEventPropertyComponents);
        auto arguments = event.getValue(apl::EventProperty::kEventPropertyArguments);

        userEventPayloadJson.AddMember(
            PRESENTATION_TOKEN_KEY, rapidjson::Value(m_aplToken.c_str(), allocator).Move(), allocator);
        userEventPayloadJson.AddMember(SOURCE_KEY, source.serialize(allocator).Move(), allocator);
        userEventPayloadJson.AddMember(ARGUMENTS_KEY, arguments.serialize(allocator).Move(), allocator);
        userEventPayloadJson.AddMember(COMPONENTS_KEY, components.serialize(allocator).Move(), allocator);

        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
        userEventPayloadJson.Accept(writer);

        aplOptions->onSendEvent(m_aplToken, sb.GetString());
        return;
    }

    if (apl::EventType::kEventTypeDataSourceFetchRequest == event.getType()) {
        rapidjson::Document fetchRequestPayloadJson(rapidjson::kObjectType);
        auto& allocator = fetchRequestPayloadJson.GetAllocator();
        auto type = event.getValue(apl::EventProperty::kEventPropertyName);
        auto payload = event.getValue(apl::EventProperty::kEventPropertyValue);

        apl::ObjectMap fetchRequest(payload.getMap());
        fetchRequest.emplace(PRESENTATION_TOKEN_KEY, m_aplToken);

        auto fetch = apl::Object(std::make_shared<apl::ObjectMap>(fetchRequest)).serialize(allocator);
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
        fetch.Accept(writer);

        aplOptions->onDataSourceFetchRequestEvent(m_aplToken, type.asString(), sb.GetString());
        return;
    }

    
    if (apl::EventType::kEventTypeExtension == event.getType()) {
        if (m_extensionManager->useAlexaExt()) {
            auto mediator = m_Root->rootConfig().getExtensionMediator();
            mediator->invokeCommand(event);
            return;
        } else {
            /**
             * Extension Events are received when registered ExtensionCommands are fired
             * https://github.com/alexa/apl-core-library/blob/master/aplcore/include/apl/content/extensioncommanddefinition.h
             */
            rapidjson::Document extensionEventPayloadJson(rapidjson::kObjectType);
            auto& allocator = extensionEventPayloadJson.GetAllocator();

            auto uri = event.getValue(apl::EventProperty::kEventPropertyExtensionURI);
            auto name = event.getValue(apl::EventProperty::kEventPropertyName);
            auto source = event.getValue(apl::EventProperty::kEventPropertySource);
            auto params = event.getValue(apl::EventProperty::kEventPropertyExtension);

            std::string sourceStr;
            std::string paramsStr;

            serializeJSONValueToString(source.serialize(allocator).Move(), &sourceStr);
            serializeJSONValueToString(params.serialize(allocator).Move(), &paramsStr);

            /**
             * If the registered ExtensionCommand requires resolution, the resultCallback should be registered with the
             * extension
             * https://github.com/alexa/apl-core-library/blob/master/aplcore/include/apl/content/extensioncommanddefinition.h#L87
             */
            auto token = ++m_SequenceNumber;
            auto resultCallback = addPendingEvent(token, event, false) ? shared_from_this() : nullptr;
            aplOptions->onExtensionEvent(m_aplToken, uri.getString(), name.getString(), sourceStr, paramsStr, token, resultCallback);
            return;
        }
    }
    
    auto msg = AplCoreViewhostMessage(EVENT_KEY);
    auto token = send(msg.setPayload(event.serialize(msg.alloc())));
    addPendingEvent(token, event);
}

bool AplCoreConnectionManager::addPendingEvent(unsigned int token, const apl::Event& event, bool isViewhostEvent) {
    // If the event had an action ref, stash the reference for future use
    auto ref = event.getActionRef();
    if (!ref.empty()) {
        m_PendingEvents.emplace(token, ref);
        ref.addTerminateCallback([this, token, isViewhostEvent](const apl::TimersPtr&) {
            auto it = m_PendingEvents.find(token);
            if (it != m_PendingEvents.end()) {
                if (isViewhostEvent) {
                    auto msg = AplCoreViewhostMessage(EVENT_TERMINATE_KEY);
                    rapidjson::Value payload(rapidjson::kObjectType);
                    payload.AddMember("token", token, msg.alloc());
                    send(msg.setPayload(std::move(payload)));
                }

                m_PendingEvents.erase(it);  // Remove the pending event
            } else {
                m_aplConfiguration->getAplOptions()->logMessage(LogLevel::WARN, __func__, "Event was not pending");
            }
        });
        return true;
    }
    return false;
}

void AplCoreConnectionManager::sendHierarchy(const std::string& messageKey, bool blocking) {
    if (m_Root) {
        auto reply = AplCoreViewhostMessage(messageKey);
        rapidjson::Value hierarchy(rapidjson::kObjectType);
        hierarchy.AddMember("hierarchy", m_Root->topComponent()->serialize(reply.alloc()), reply.alloc());

        rapidjson::Value displayedChildrenHierarchy = buildDisplayedChildrenHierarchy(m_Root->topComponent(), reply);
        hierarchy.AddMember("displayedChildrenHierarchy", displayedChildrenHierarchy, reply.alloc());

        if (blocking) {
            blockingSend(reply.setPayload(std::move(hierarchy)));
        } else {
            send(reply.setPayload(std::move(hierarchy)));
        }
    }
}

rapidjson::Value
AplCoreConnectionManager::buildDisplayedChildrenHierarchy(const apl::ComponentPtr& component, AplCoreViewhostMessage& message) {
    rapidjson::Value displayedChildrenHierarchy(rapidjson::kObjectType);
    std::vector<apl::ComponentPtr> stack;
    stack.push_back(component);

    while(!stack.empty()) {
        rapidjson::Value displayedChildrenUniqueIds(rapidjson::kArrayType);
        apl::ComponentPtr node = stack.back();
        stack.pop_back();
        auto count = node->getDisplayedChildCount();
        for (size_t i = 0; i < count; i++) {
            auto childComponent = node->getDisplayedChildAt(i);
            auto str = childComponent->getUniqueId();
            displayedChildrenUniqueIds.PushBack(rapidjson::Value{}.SetString(str.c_str(), str.length(), message.alloc()), message.alloc());
            stack.push_back(childComponent);
        }
        displayedChildrenHierarchy.AddMember(
                rapidjson::Value{}.SetString(node->getUniqueId().c_str(), node->getUniqueId().length(),message.alloc()),
                displayedChildrenUniqueIds,
                message.alloc());
    }
    return displayedChildrenHierarchy;
}

void AplCoreConnectionManager::processDirty(const std::set<apl::ComponentPtr>& dirty) {
    std::map<std::string, rapidjson::Value> tempDirty;
    auto msg = AplCoreViewhostMessage(DIRTY_KEY);

    for (auto& component : dirty) {
        if (component->getDirty().count(apl::kPropertyNotifyChildrenChanged)) {
            auto notify = component->getCalculated(apl::kPropertyNotifyChildrenChanged);
            const auto& changed = notify.getArray();
            // Whenever we get NotifyChildrenChanged we get 2 types of action
            // Either insert or delete. The delete will happen on the viewhost level
            // However, insert needs the full serialized component from core & will be initalized
            // on apl-client side
            for (size_t i = 0; i < changed.size(); i++) {
                auto newChildId = changed.at(i).get("uid").asString();
                auto newChildIndex = changed.at(i).get("index").asInt();
                auto action = changed.at(i).get("action").asString();
                if (action == "insert") {
                    auto newComponent = component->getChildAt(newChildIndex);
                    rapidjson::Value newComponentHierarchy = newComponent->serialize(msg.alloc());
                    rapidjson::Value displayedChildrenHierarchy = buildDisplayedChildrenHierarchy(newComponent, msg);
                    newComponentHierarchy.AddMember("displayedChildrenHierarchy", displayedChildrenHierarchy, msg.alloc());
                    tempDirty[newChildId] = newComponentHierarchy;
                }
            }
            if (tempDirty.find(component->getUniqueId()) == tempDirty.end()) {
                // notify children change needs to update displayed children ids
                rapidjson::Value dirtyWithChildChange = component->serializeDirty(msg.alloc());
                rapidjson::Value displayedChildrenHierarchy = buildDisplayedChildrenHierarchy(component, msg);
                dirtyWithChildChange.AddMember("displayedChildrenHierarchy", displayedChildrenHierarchy, msg.alloc());
                tempDirty[component->getUniqueId()] = dirtyWithChildChange;
            }
        }
        if (component->getDirty().count(apl::kPropertyGraphic)) {
            // for graphic component, apl-client need walk into graphicPtr to get dirty and dirtyPropertyKeys.
            auto object = component->getCalculated(apl::kPropertyGraphic);
            if (object.is<apl::Graphic>()) {
                auto graphic = object.get<apl::Graphic>();
                rapidjson::Value vectorGraphicComponent = component->serializeDirty(msg.alloc());
                rapidjson::Value dirtyGraphicElement(rapidjson::kArrayType);
                for (auto& graphicDirty : graphic->getDirty()) {
                    rapidjson::Value serializedGraphicElement = graphicDirty->serialize(msg.alloc());
                    rapidjson::Value dirtyPropertyKeys(rapidjson::kArrayType);
                    for (auto& dirtyPropertyKey : graphicDirty->getDirtyProperties()) {
                        dirtyPropertyKeys.PushBack(dirtyPropertyKey, msg.alloc());
                    }
                    serializedGraphicElement.AddMember("dirtyProperties", dirtyPropertyKeys, msg.alloc());
                    dirtyGraphicElement.PushBack(serializedGraphicElement, msg.alloc());
                }
                if (vectorGraphicComponent.HasMember("graphic") && vectorGraphicComponent["graphic"].IsObject()) {
                    vectorGraphicComponent["graphic"].AddMember("dirty", dirtyGraphicElement, msg.alloc());
                }
                tempDirty[component->getUniqueId()] = vectorGraphicComponent;
            }
        }
        if (tempDirty.find(component->getUniqueId()) == tempDirty.end()) {
            tempDirty.emplace(component->getUniqueId(), component->serializeDirty(msg.alloc()));
        }
    }

    rapidjson::Value array(rapidjson::kArrayType);
    for (auto rit = tempDirty.rbegin(); rit != tempDirty.rend(); rit++) {
        auto uid = rit->first;
        auto& update = rit->second;

        array.PushBack(update.Move(), msg.alloc());
    }
    send(msg.setPayload(std::move(array)));
}

void AplCoreConnectionManager::coreFrameUpdate() {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "coreFrameUpdateFailed", "Root context is null");
        return;
    }
    auto now = getCurrentTime() - m_StartTime;
    m_Root->updateTime(now.count(), getCurrentTime().count());
    m_Root->setLocalTimeAdjustment(aplOptions->getTimezoneOffset().count());
    std::dynamic_pointer_cast<AplCoreAudioPlayerFactory>(m_Root->getRootConfig().getAudioPlayerFactory())->tick(*this);

    m_Root->clearPending();

    while (m_Root->hasEvent()) {
        processEvent(m_Root->popEvent());
    }

    if (m_Root->isDirty()) {
        processDirty(m_Root->getDirty());
        m_Root->clearDirty();
    }

    handleScreenLock();
}

void AplCoreConnectionManager::onUpdateTick() {
    if (m_Root) {
        coreFrameUpdate();
        // Check regularly as something like timed-out fetch requests could come up.
        checkAndSendDataSourceErrors();
    }
}

apl::Rect AplCoreConnectionManager::convertJsonToScaledRect(const rapidjson::Value& jsonNode) {
    const float scale = m_AplCoreMetrics->toCore(1.0f);
    const float x = jsonNode[X_KEY].IsNumber() ? jsonNode[X_KEY].GetFloat() : 0.0f;
    const float y = jsonNode[Y_KEY].IsNumber() ? jsonNode[Y_KEY].GetFloat() : 0.0f;
    const float width = jsonNode[WIDTH_KEY].IsNumber() ? jsonNode[WIDTH_KEY].GetFloat() : 0.0f;
    const float height = jsonNode[HEIGHT_KEY].IsNumber() ? jsonNode[HEIGHT_KEY].GetFloat() : 0.0f;

    return apl::Rect(x * scale, y * scale, width * scale, height * scale);
}

void AplCoreConnectionManager::checkAndSendDataSourceErrors() {
    if (!m_Root) return;

    std::vector<apl::Object> errorArray;

    for (auto& type : KNOWN_DATA_SOURCES) {
        auto provider = m_Root->getRootConfig().getDataSourceProvider(type);

        if (provider) {
            auto pendingErrors = provider->getPendingErrors();
            if (!pendingErrors.empty() && pendingErrors.isArray()) {
                errorArray.insert(errorArray.end(), pendingErrors.getArray().begin(), pendingErrors.getArray().end());
            }
        }
    }

    auto errors = apl::Object(std::make_shared<apl::ObjectArray>(errorArray));

    if (!errors.empty()) {
        auto errorEvent = std::make_shared<apl::ObjectMap>();
        errorEvent->emplace(PRESENTATION_TOKEN_KEY, m_aplToken);
        errorEvent->emplace(ERRORS_KEY, errors);

        rapidjson::Document runtimeErrorPayloadJson(rapidjson::kObjectType);
        auto& allocator = runtimeErrorPayloadJson.GetAllocator();
        auto runtimeError = apl::Object(errorEvent).serialize(allocator);

        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
        runtimeError.Accept(writer);

        m_aplConfiguration->getAplOptions()->onRuntimeErrorEvent(m_aplToken, sb.GetString());
    }
}

const std::string AplCoreConnectionManager::getAPLToken() {
    return m_aplToken;
}

void AplCoreConnectionManager::serializeJSONValueToString(const rapidjson::Value& documentNode, std::string* value) {
    rapidjson::StringBuffer stringBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

    if (!documentNode.Accept(writer)) {
        auto aplOptions = m_aplConfiguration->getAplOptions();
        aplOptions->logMessage(LogLevel::ERROR, "serializeJSONValueToStringFailed", "acceptFailed");
        return;
    }

    *value = stringBuffer.GetString();
}

void AplCoreConnectionManager::addExtensions(
        std::unordered_set<std::shared_ptr<AplCoreExtensionInterface>> extensions) {
    for (auto& extension : extensions) {
        extension->setEventHandler(shared_from_this());
        m_extensionManager->addExtension(extension);
    }
}

void AplCoreConnectionManager::addAlexaExtExtensions(
        const std::unordered_set<alexaext::ExtensionPtr>& extensions,
        const alexaext::ExtensionRegistrarPtr& registrar,
        const AlexaExtExtensionExecutorPtr& executor
    ) {
    for (auto& extension : extensions) {
        m_extensionManager->addAlexaExtExtension(extension);
    }
    m_extensionManager->setExtensionRegistrar(registrar);
    m_extensionManager->setExtensionExecutor(executor);
}

std::shared_ptr<AplCoreExtensionInterface> AplCoreConnectionManager::getExtension(const std::string& uri) {
    return m_extensionManager->getExtension(uri);
}

alexaext::ExtensionPtr AplCoreConnectionManager::getAlexaExtExtension(const std::string& uri) {
    return m_extensionManager->getAlexaExtExtension(uri);
}

void AplCoreConnectionManager::reset() {
    m_aplToken = "";
    m_Root.reset();
    m_Content.reset();
}

void AplCoreConnectionManager::handleIsCharacterValid(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleIsCharacterValidFailed", "Root context is null");
        return;
    }

    auto messageId = payload["messageId"].GetString();
    if (!messageId) {
        aplOptions->logMessage(
            LogLevel::ERROR, "handleIsCharacterValidFailed", std::string("Payload does not contain messageId"));
        sendError("Payload does not contain messageId");
        return;
    }

    auto character = payload["character"].GetString();
    if (!character) {
        aplOptions->logMessage(
            LogLevel::ERROR, "handleIsCharacterValidFailed", std::string("Payload does not contain character"));
        sendError("Payload does not contain character");
        return;
    }

    auto componentId = payload["componentId"].GetString();
    if (!componentId) {
        aplOptions->logMessage(
            LogLevel::ERROR, "handleIsCharacterValidFailed", std::string("Payload does not contain componentId"));
        sendError("Payload does not contain componentId");
        return;
    }
    auto component = m_Root->findComponentById(componentId);
    if (!component) {
        aplOptions->logMessage(
            LogLevel::ERROR,
            "handleIsCharacterValidFailed",
            std::string("Unable to find component with id: ") + componentId);
        sendError("Unable to find component");
        return;
    }

    auto result = component->isCharacterValid(character[0]);

    auto resultMessage = AplCoreViewhostMessage("isCharacterValid");
    auto& alloc = resultMessage.alloc();
    rapidjson::Value resultMessageValue(rapidjson::kObjectType);

    rapidjson::Value messageIdValue;
    messageIdValue.SetString(messageId, alloc);
    resultMessageValue.AddMember("messageId", messageIdValue, alloc);

    resultMessageValue.AddMember("valid", result, alloc);

    rapidjson::Value componentIdValue;
    componentIdValue.SetString(componentId, alloc);
    resultMessageValue.AddMember("componentId", componentIdValue, alloc);

    resultMessage.setPayload(std::move(resultMessageValue));
    send(resultMessage);
}

void AplCoreConnectionManager::handleReInflate(const rapidjson::Value& payload) {
    auto aplOptions = m_aplConfiguration->getAplOptions();
    if (!m_Root) {
        aplOptions->logMessage(LogLevel::ERROR, "handleIsCharacterValidFailed", "Root context is null");
        return;
    }

    if (m_Content->isWaiting()) {
        if (!loadPackage(m_Content)) {
            aplOptions->logMessage(
                    LogLevel::WARN, __func__, "Unable to reload content.");
            sendError("Content failed to reload");
            return;
        }
    }

    if (!registerRequestedExtensions()) {
        // If using AlexaExt: Required extensions have not loaded
        aplOptions->logMessage(LogLevel::ERROR, "handleReinflate", "Required extensions have not loaded");
        sendError("Required extensions have not loaded");
        return;
    }

    m_Root->reinflate();

    // update component hierarchy
    sendHierarchy(HIERARCHY_KEY);
}

void AplCoreConnectionManager::handleReHierarchy(const rapidjson::Value& payload) {
    // send component hierarchy
    sendHierarchy(REHIERARCHY_KEY, true);
}

void AplCoreConnectionManager::updateConfigurationChange(const apl::ConfigurationChange& configurationChange) {
    m_ConfigurationChange.mergeConfigurationChange(configurationChange);
}

void AplCoreConnectionManager::handleExtensionMessage(const rapidjson::Value& payload) {
    auto const& uriMember = payload.FindMember("uri")->value;
    auto const& nameMember = payload.FindMember("name")->value;
    auto const& fastModeMember = payload.FindMember("fastMode")->value;
    auto const aplOptions = m_aplConfiguration->getAplOptions();
    if (!uriMember.IsString()
        || !nameMember.IsString()
        || !fastModeMember.IsBool()) {
        aplOptions->logMessage(LogLevel::ERROR, "handleExtensionMessageFailed", "Could not parse extensionMessage");
        return;
    }
    auto const uri = uriMember.GetString();
    auto const name = nameMember.GetString();
    auto const fastMode = fastModeMember.GetBool();

    auto const& dataMember = payload.FindMember("data")->value;
    apl::ObjectMap data;
    for (auto itr = dataMember.MemberBegin(); itr != dataMember.MemberEnd(); ++itr) {
        if (!itr->value.IsString()) {
            aplOptions->logMessage(LogLevel::ERROR, "handleExtensionMessageFailed",
                                   "Could not parse extensionMessage data");
            continue;
        }
        data.emplace(itr->name.GetString(), itr->value.GetString());
    }
    invokeExtensionEventHandler(uri, name, data, fastMode);
}

}  // namespace APLClient
