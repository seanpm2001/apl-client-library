/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

export declare enum PropertyKey {
    kPropertyScrollDirection = 0,
    kPropertyAccessibilityActions = 1,
    kPropertyAccessibilityActionsAssigned = 2,
    kPropertyAccessibilityLabel = 3,
    kPropertyAlign = 4,
    kPropertyAlignItems = 5,
    kPropertyAlignSelf = 6,
    kPropertyAudioTrack = 7,
    kPropertyAutoplay = 8,
    kPropertyMuted = 9,
    kPropertyBackgroundColor = 10,
    kPropertyBackgroundAssigned = 11,
    kPropertyBackground = 12,
    kPropertyBorderBottomLeftRadius = 13,
    kPropertyBorderBottomRightRadius = 14,
    kPropertyBorderColor = 15,
    kPropertyBorderRadius = 16,
    kPropertyBorderRadii = 17,
    kPropertyBorderStrokeWidth = 18,
    kPropertyBorderTopLeftRadius = 19,
    kPropertyBorderTopRightRadius = 20,
    kPropertyBorderWidth = 21,
    kPropertyBottom = 22,
    kPropertyBounds = 23,
    kPropertyCenterId = 24,
    kPropertyCenterIndex = 25,
    kPropertyChildHeight = 26,
    kPropertyChildWidth = 27,
    kPropertyChecked = 28,
    kPropertyColor = 29,
    kPropertyColorKaraokeTarget = 30,
    kPropertyColorNonKaraoke = 31,
    kPropertyCurrentPage = 32,
    kPropertyDescription = 33,
    kPropertyDirection = 34,
    kPropertyDisabled = 35,
    kPropertyDisplay = 36,
    kPropertyDrawnBorderWidth = 37,
    kPropertyEmbeddedDocument = 38,
    kPropertyEnd = 39,
    kPropertyEntities = 40,
    kPropertyEnvironment = 41,
    kPropertyFastScrollScale = 42,
    kPropertyFilters = 43,
    kPropertyFirstId = 44,
    kPropertyFirstIndex = 45,
    kPropertyFocusable = 46,
    kPropertyFontFamily = 47,
    kPropertyFontSize = 48,
    kPropertyFontStyle = 49,
    kPropertyFontWeight = 50,
    kPropertyHandleTick = 51,
    kPropertyHighlightColor = 52,
    kPropertyHint = 53,
    kPropertyHintColor = 54,
    kPropertyHintStyle = 55,
    kPropertyHintWeight = 56,
    kPropertyGestures = 57,
    kPropertyGraphic = 58,
    kPropertyGrow = 59,
    kPropertyHandleKeyDown = 60,
    kPropertyHandleKeyUp = 61,
    kPropertyHeight = 62,
    kPropertyId = 63,
    kPropertyInitialPage = 64,
    kPropertyInnerBounds = 65,
    kPropertyItemsPerCourse = 66,
    kPropertyJustifyContent = 67,
    kPropertyKeyboardBehaviorOnFocus = 68,
    kPropertyKeyboardType = 69,
    kPropertyLayoutDirection = 70,
    kPropertyLayoutDirectionAssigned = 71,
    kPropertyLeft = 72,
    kPropertyLetterSpacing = 73,
    kPropertyLineHeight = 74,
    kPropertyMaxHeight = 75,
    kPropertyMaxLength = 76,
    kPropertyMaxLines = 77,
    kPropertyMaxWidth = 78,
    kPropertyMediaBounds = 79,
    kPropertyMediaState = 80,
    kPropertyMinHeight = 81,
    kPropertyMinWidth = 82,
    kPropertyNavigation = 83,
    kPropertyNextFocusDown = 84,
    kPropertyNextFocusForward = 85,
    kPropertyNextFocusLeft = 86,
    kPropertyNextFocusRight = 87,
    kPropertyNextFocusUp = 88,
    kPropertyNotifyChildrenChanged = 89,
    kPropertyNumbered = 90,
    kPropertyNumbering = 91,
    kPropertyOnBlur = 92,
    kPropertyOnCancel = 93,
    kPropertyOnChildrenChanged = 94,
    kPropertyOnDown = 95,
    kPropertyOnEnd = 96,
    kPropertyOnFail = 97,
    kPropertyOnFocus = 98,
    kPropertyOnLoad = 99,
    kPropertyOnMount = 100,
    kPropertyOnMove = 101,
    kPropertyOnSpeechMark = 102,
    kPropertyHandlePageMove = 103,
    kPropertyOnPageChanged = 104,
    kPropertyOnPause = 105,
    kPropertyOnPlay = 106,
    kPropertyOnPress = 107,
    kPropertyOnScroll = 108,
    kPropertyOnSubmit = 109,
    kPropertyOnTextChange = 110,
    kPropertyOnUp = 111,
    kPropertyOnTimeUpdate = 112,
    kPropertyOnTrackFail = 113,
    kPropertyOnTrackReady = 114,
    kPropertyOnTrackUpdate = 115,
    kPropertyOpacity = 116,
    kPropertyOverlayColor = 117,
    kPropertyOverlayGradient = 118,
    kPropertyPadding = 119,
    kPropertyPaddingBottom = 120,
    kPropertyPaddingEnd = 121,
    kPropertyPaddingLeft = 122,
    kPropertyPaddingRight = 123,
    kPropertyPaddingTop = 124,
    kPropertyPaddingStart = 125,
    kPropertyPageDirection = 126,
    kPropertyPageId = 127,
    kPropertyPageIndex = 128,
    kPropertyParameters = 129,
    kPropertyPlayingState = 130,
    kPropertyPosition = 131,
    kPropertyPreserve = 132,
    kPropertyRangeKaraokeTarget = 133,
    kPropertyResourceId = 134,
    kPropertyResourceOnFatalError = 135,
    kPropertyResourceState = 136,
    kPropertyResourceType = 137,
    kPropertyRight = 138,
    kPropertyRole = 139,
    kPropertyScale = 140,
    kPropertyScrollAnimation = 141,
    kPropertyScrollOffset = 142,
    kPropertyScrollPercent = 143,
    kPropertyScrollPosition = 144,
    kPropertySecureInput = 145,
    kPropertySelectOnFocus = 146,
    kPropertyShadowColor = 147,
    kPropertyShadowHorizontalOffset = 148,
    kPropertyShadowRadius = 149,
    kPropertyShadowVerticalOffset = 150,
    kPropertyShrink = 151,
    kPropertySize = 152,
    kPropertySnap = 153,
    kPropertySource = 154,
    kPropertySpacing = 155,
    kPropertySpeech = 156,
    kPropertyStart = 157,
    kPropertySubmitKeyType = 158,
    kPropertyText = 159,
    kPropertyTextAlign = 160,
    kPropertyTextAlignAssigned = 161,
    kPropertyTextAlignVertical = 162,
    kPropertyLang = 163,
    kPropertyTrackCount = 164,
    kPropertyTrackCurrentTime = 165,
    kPropertyTrackDuration = 166,
    kPropertyTrackEnded = 167,
    kPropertyTrackIndex = 168,
    kPropertyTrackPaused = 169,
    kPropertyTrackState = 170,
    kPropertyTransform = 171,
    kPropertyTransformAssigned = 172,
    kPropertyTop = 173,
    kPropertyUser = 174,
    kPropertyWidth = 175,
    kPropertyOnCursorEnter = 176,
    kPropertyOnCursorExit = 177,
    kPropertyLaidOut = 178,
    kPropertyValidCharacters = 179,
    kPropertyVisualHash = 180,
    kPropertyWrap = 181
}
