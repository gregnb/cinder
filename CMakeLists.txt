cmake_minimum_required(VERSION 3.16)

project(Cinder VERSION 0.1.0 LANGUAGES CXX OBJCXX)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Constants.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/Constants.h
    @ONLY
)

set(CINDER_TEAM_ID "$ENV{CINDER_TEAM_ID}" CACHE STRING
    "Apple team ID used for macOS entitlements")
if(CINDER_TEAM_ID)
    set(CINDER_ENTITLEMENTS_DICT
"    <key>keychain-access-groups</key>
    <array>
        <string>${CINDER_TEAM_ID}.com.cinder.wallet</string>
    </array>")
else()
    set(CINDER_ENTITLEMENTS_DICT "")
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Cinder.entitlements.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/Cinder.entitlements
    @ONLY
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 packages (fallback to Qt5 if Qt6 not found)
find_package(Qt6 COMPONENTS Core Widgets Charts Sql Network Concurrent Test LinguistTools PrintSupport QUIET)
if (NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Core Widgets REQUIRED)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

# Add SVG support (must come after QT_VERSION_MAJOR is set)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Svg QUIET)
if(Qt${QT_VERSION_MAJOR}Svg_FOUND)
    set(QT_SVG_LIBRARY Qt${QT_VERSION_MAJOR}::Svg)
endif()

# libsodium (Ed25519, HMAC-SHA512, SHA-256, CSPRNG)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)
pkg_check_modules(QRENCODE REQUIRED libqrencode)
pkg_check_modules(HIDAPI REQUIRED hidapi)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
find_package(ZLIB REQUIRED)
link_directories(${SODIUM_LIBRARY_DIRS} ${QRENCODE_LIBRARY_DIRS} ${HIDAPI_LIBRARY_DIRS} ${LIBUSB_LIBRARY_DIRS})

# Source files
set(PROJECT_SOURCES
    src/main.cpp
    src/Theme.h
    src/StyleLoader.h
    src/CinderWalletApp.cpp
    src/CinderWalletApp.h
    src/app/AppShell.cpp
    src/app/AppSession.cpp
    src/app/HardwareWalletCoordinator.cpp
    src/widgets/ContentBorderWidget.h
    src/widgets/NotificationFlyout.cpp
    src/widgets/NotificationFlyout.h
    src/widgets/SplineChart.cpp
    src/widgets/SplineChart.h
    src/widgets/TpsBarChart.cpp
    src/widgets/TpsBarChart.h
    src/widgets/CardListItem.cpp
    src/widgets/CardListItem.h
    src/widgets/TokenDropdown.cpp
    src/widgets/ToggleSwitch.cpp
    src/widgets/ToggleSwitch.h
    src/widgets/TokenDropdown.h
    src/widgets/UploadWidget.cpp
    src/widgets/UploadWidget.h
    src/widgets/AccountSelector.cpp
    src/widgets/AccountSelector.h
    src/widgets/Dropdown.cpp
    src/widgets/Dropdown.h
    src/widgets/AmountInput.cpp
    src/widgets/AmountInput.h
    src/widgets/TxStatusAnimationWidget.cpp
    src/widgets/TxStatusAnimationWidget.h
    src/widgets/ComputeUnitsBar.cpp
    src/widgets/ComputeUnitsBar.h
    src/widgets/FlapDisplay.cpp
    src/widgets/FlapDisplay.h
    src/widgets/AddressInput.cpp
    src/widgets/AddressInput.h
    src/widgets/AddressLink.cpp
    src/widgets/AddressLink.h
    src/widgets/QrCodeWidget.cpp
    src/widgets/QrCodeWidget.h
    src/widgets/PillButtonGroup.h
    src/widgets/PillButtonGroup.cpp
    src/widgets/TabBar.cpp
    src/widgets/TabBar.h
    src/widgets/StyledCalendar.cpp
    src/widgets/StyledCalendar.h
    src/features/dashboard/DashboardPage.cpp
    src/features/dashboard/DashboardPage.h
    src/features/dashboard/DashboardHandler.cpp
    src/features/dashboard/DashboardHandler.h
    src/features/sendreceive/SendReceivePage.cpp
    src/features/sendreceive/SendReceivePage.h
    src/features/sendreceive/SendReceiveForm.cpp
    src/features/sendreceive/SendReceiveReview.cpp
    src/features/sendreceive/SendReceiveReviewActions.cpp
    src/features/sendreceive/SendReceiveNonce.cpp
    src/features/sendreceive/SendReceiveCreateToken.cpp
    src/features/sendreceive/SendReceiveCreateTokenReview.cpp
    src/features/sendreceive/SendReceiveBurnToken.cpp
    src/features/sendreceive/SendReceiveCloseAccounts.cpp
    src/features/sendreceive/SendReceiveMintToken.cpp
    src/features/sendreceive/SendReceiveSuccess.cpp
    src/features/sendreceive/SendReceiveHandler.cpp
    src/features/sendreceive/SendReceiveHandlerCreateTokenFlow.cpp
    src/features/sendreceive/SendReceiveHandlerFlow.cpp
    src/features/sendreceive/SendReceiveHandler.h
    src/features/sendreceive/CreateTokenHandler.cpp
    src/features/sendreceive/CreateTokenHandler.h
    src/features/sendreceive/MintTokenHandler.cpp
    src/features/sendreceive/MintTokenHandler.h
    src/features/sendreceive/BurnTokenHandler.cpp
    src/features/sendreceive/BurnTokenHandler.h
    src/features/sendreceive/CloseTokenAccountsHandler.cpp
    src/features/sendreceive/CloseTokenAccountsHandler.h
    src/features/staking/StakingPage.cpp
    src/features/staking/StakingPage.h
    src/features/staking/StakeStatusRail.cpp
    src/features/staking/StakeStatusRail.h
    src/features/staking/StakingHandler.cpp
    src/features/staking/StakingHandler.h
    src/features/swap/SwapPage.cpp
    src/features/swap/SwapPage.h
    src/features/swap/SwapHandler.cpp
    src/features/swap/SwapHandler.h
    src/features/settings/SettingsPage.cpp
    src/features/settings/SettingsPage.h
    src/features/settings/SettingsHandler.cpp
    src/features/settings/SettingsHandler.h
    src/features/setup/SetupPage.cpp
    src/features/setup/SetupPage.h
    src/features/setup/SetupHandler.cpp
    src/features/setup/SetupHandler.h
    src/features/activity/ActivityPage.cpp
    src/features/activity/ActivityPage.h
    src/features/activity/ActivityHandler.cpp
    src/features/activity/ActivityHandler.h
    src/features/addressbook/AddressBookPage.cpp
    src/features/addressbook/AddressBookPage.h
    src/features/addressbook/AddressBookHandler.cpp
    src/features/addressbook/AddressBookHandler.h
    src/features/txlookup/TxLookupPage.cpp
    src/features/txlookup/TxLookupPage.h
    src/features/txlookup/TxLookupHandler.cpp
    src/features/txlookup/TxLookupHandler.h
    src/features/assets/AssetsPage.cpp
    src/features/assets/AssetsPage.h
    src/features/assets/AssetsHandler.cpp
    src/features/assets/AssetsHandler.h
    src/features/terminal/TerminalPage.cpp
    src/features/terminal/TerminalPage.h
    src/features/terminal/TerminalHandler.cpp
    src/features/terminal/TerminalHandlerData.cpp
    src/features/terminal/TerminalHandlerRpcNonce.cpp
    src/features/terminal/TerminalHandlerStakeValidator.cpp
    src/features/terminal/TerminalHandlerSwapPrice.cpp
    src/features/terminal/TerminalHandler.h
    src/features/terminal/TerminalAsyncOp.cpp
    src/features/terminal/TerminalAsyncOp.h
    src/features/terminal/TerminalCommandContext.cpp
    src/features/terminal/TerminalCommandContext.h
    src/features/terminal/TerminalHandlerTx.cpp
    src/features/terminal/TerminalHandlerTxRender.cpp
    src/features/terminal/TerminalHandlerTokenAccount.cpp
    src/features/terminal/TerminalHandlerNetwork.cpp
    src/features/terminal/TerminalHandlerCommon.h
    src/features/terminal/TerminalRequestTracker.cpp
    src/features/terminal/TerminalRequestTracker.h
    src/features/terminal/TerminalSecretFlow.cpp
    src/features/terminal/TerminalSecretFlow.h
    src/features/terminal/TerminalCompletion.cpp
    src/features/terminal/TerminalCompletion.h
    src/features/terminal/TerminalCommandCatalog.cpp
    src/features/terminal/TerminalCommandCatalog.h
    src/features/lockscreen/LockScreen.cpp
    src/features/lockscreen/LockScreen.h
    src/features/lockscreen/LockScreenHandler.cpp
    src/features/lockscreen/LockScreenHandler.h
    src/features/wallets/WalletsPage.cpp
    src/features/wallets/WalletsPage.h
    src/features/wallets/WalletsHandler.cpp
    src/features/wallets/WalletsHandler.h
    src/features/agents/AgentsPage.cpp
    src/features/agents/AgentsPage.h
    src/features/agents/AgentsHandler.cpp
    src/features/agents/AgentsHandler.h
    src/features/agents/ApprovalExecutor.cpp
    src/features/agents/ApprovalExecutor.h
    src/agents/McpServer.cpp
    src/agents/McpServer.h
    src/agents/McpClient.cpp
    src/agents/McpClient.h
    src/agents/ModelProvider.h
    src/agents/ModelProvider.cpp
    src/agents/models/codex/CodexProvider.cpp
    src/agents/models/codex/CodexProvider.h
    src/agents/models/claude/ClaudeProvider.cpp
    src/agents/models/claude/ClaudeProvider.h
    src/agents/models/claudecodecli/ClaudeCodeCliProvider.cpp
    src/agents/models/claudecodecli/ClaudeCodeCliProvider.h
    src/agents/AiClient.cpp
    src/agents/AiClient.h
    src/agents/McpToolDefs.h
    src/db/Database.cpp
    src/db/Database.h
    src/db/DbUtil.h
    src/db/DbUtil.cpp
    src/db/TransactionDb.cpp
    src/db/TransactionDb.h
    src/db/ContactDb.cpp
    src/db/ContactDb.h
    src/db/TokenAccountDb.cpp
    src/db/TokenAccountDb.h
    src/db/PortfolioDb.cpp
    src/db/PortfolioDb.h
    src/db/WalletDb.cpp
    src/db/WalletDb.h
    src/db/IdlDb.cpp
    src/db/IdlDb.h
    src/db/NonceDb.cpp
    src/db/NonceDb.h
    src/db/ValidatorCacheDb.cpp
    src/db/ValidatorCacheDb.h
    src/db/StakeAccountDb.cpp
    src/db/StakeAccountDb.h
    src/db/StakeAccountRewardDb.cpp
    src/db/StakeAccountRewardDb.h
    src/db/NetworkStatsDb.cpp
    src/db/NetworkStatsDb.h
    src/db/SyncStateDb.cpp
    src/db/SyncStateDb.h
    src/db/NotificationDb.cpp
    src/db/NotificationDb.h
    src/db/McpDb.cpp
    src/db/McpDb.h
    src/util/TimeUtils.h
    src/services/SolanaApi.cpp
    src/services/SolanaApi.h
    src/services/JupiterApi.cpp
    src/services/JupiterApi.h
    src/services/model/JupiterQuote.h
    src/services/ValidatorService.cpp
    src/services/ValidatorService.h
    src/services/AvatarCache.cpp
    src/services/AvatarCache.h
    src/services/NetworkStatsService.cpp
    src/services/NetworkStatsService.h
    src/services/model/NetworkStats.h
    src/services/PriceService.cpp
    src/services/PriceService.h
    src/services/IrysApi.cpp
    src/services/IrysApi.h
    src/services/PortfolioService.cpp
    src/services/PortfolioService.h
    src/services/SyncService.cpp
    src/services/SyncService.h
    src/services/BackfillService.cpp
    src/services/BackfillService.h
    src/services/TokenMetadataService.cpp
    src/services/TokenMetadataService.h
    src/services/IdlRegistry.cpp
    src/services/IdlRegistry.h
    src/services/model/SignatureInfo.h
    src/services/model/StakeRewardInfo.h
    src/services/model/TransactionResponse.h
    src/services/model/SimulationResponse.h
    src/services/model/PriorityFee.h
    src/services/model/PriceData.h
    src/services/model/ValidatorInfo.h
    src/services/model/StakeAccountInfo.h
    src/models/Dashboard.h
    src/models/DashboardStakeItem.h
    src/models/AddressBook.h
    src/models/Assets.h
    src/models/Activity.h
    src/models/TxLookup.h
    src/models/Staking.h
    src/models/Swap.h
    src/models/Agents.h
    src/models/SendReceive.h
    src/tx/ProgramIds.h
    src/tx/MetaplexMetadata.h
    src/tx/Token2022Metadata.h
    src/tx/StakeInstruction.h
    src/tx/NonceAccount.h
    src/util/ContactResolver.cpp
    src/util/ContactResolver.h
    src/util/FrameRecorder.cpp
    src/util/FrameRecorder.h
    src/util/SolanaErrorParser.h
    src/tx/Base58.h
    src/tx/CompactU16.h
    src/tx/AccountMeta.h
    src/tx/TransactionInstruction.h
    src/tx/TransactionBuilder.h
    src/tx/TransactionBuilder.cpp
    src/tx/TransactionExecutor.h
    src/tx/TransactionExecutor.cpp
    src/tx/TxExecutionConstants.h
    src/tx/SystemInstruction.h
    src/tx/SystemOperationBuilder.h
    src/tx/SystemOperationBuilder.cpp
    src/tx/TokenInstruction.h
    src/tx/TokenEncodingUtils.h
    src/tx/Token2022Types.h
    src/tx/Token2022Instruction.h
    src/tx/TokenMetadataInstruction.h
    src/tx/TokenGroupInstruction.h
    src/tx/ComputeBudgetInstruction.h
    src/tx/KnownTokens.h
    src/tx/InstructionDecoder.h
    src/tx/TokenOperationBuilder.h
    src/tx/TokenOperationBuilder.cpp
    src/tx/TxParseUtils.cpp
    src/tx/TxClassifier.h
    src/tx/AnchorIdl.h
    src/tx/AssociatedTokenInstruction.h
    src/tx/AssociatedTokenInstruction.cpp
    src/crypto/PdaDerivation.h
    src/crypto/Keypair.h
    src/crypto/Keypair.cpp
    src/crypto/Mnemonic.h
    src/crypto/Mnemonic.cpp
    src/crypto/HDDerivation.h
    src/crypto/Pbkdf2.h
    src/crypto/WalletCrypto.h
    src/crypto/WalletCrypto.cpp
    src/crypto/bip39_wordlist.h
    src/crypto/Signer.h
    src/crypto/Signer.cpp
    src/crypto/SoftwareSigner.h
    src/crypto/SoftwareSigner.cpp
    src/crypto/HardwareWalletPlugin.h
    src/crypto/SignerManager.h
    src/crypto/SignerManager.cpp
    src/crypto/plugin/ledger/LedgerTransport.h
    src/crypto/plugin/ledger/LedgerTransport.cpp
    src/crypto/plugin/ledger/LedgerSigner.h
    src/crypto/plugin/ledger/LedgerSigner.cpp
    src/crypto/plugin/ledger/LedgerPlugin.h
    src/crypto/plugin/ledger/LedgerPlugin.cpp
    src/crypto/plugin/trezor/TrezorProtobuf.h
    src/crypto/plugin/trezor/TrezorProtobuf.cpp
    src/crypto/plugin/trezor/TrezorTransport.h
    src/crypto/plugin/trezor/TrezorTransport.cpp
    src/crypto/plugin/trezor/TrezorHidTransport.h
    src/crypto/plugin/trezor/TrezorHidTransport.cpp
    src/crypto/plugin/trezor/TrezorUsbTransport.h
    src/crypto/plugin/trezor/TrezorUsbTransport.cpp
    src/crypto/plugin/trezor/TrezorPlugin.h
    src/crypto/plugin/trezor/TrezorPlugin.cpp
    src/crypto/plugin/trezor/TrezorSigner.h
    src/crypto/plugin/trezor/TrezorSigner.cpp
    src/crypto/plugin/lattice/LatticeCrypto.h
    src/crypto/plugin/lattice/LatticeCrypto.mm
    src/crypto/plugin/lattice/LatticeTransport.h
    src/crypto/plugin/lattice/LatticeTransport.cpp
    src/crypto/plugin/lattice/LatticePlugin.h
    src/crypto/plugin/lattice/LatticePlugin.cpp
    src/crypto/plugin/lattice/LatticeSigner.h
    src/crypto/plugin/lattice/LatticeSigner.cpp
)

# macOS-specific sources
if(APPLE)
    list(APPEND PROJECT_SOURCES
        src/util/MacUtils.mm
        src/util/MacUtils.h
    )
endif()

# Qt Resource file (icons and images compiled into binary)
set(PROJECT_RESOURCES
    resources.qrc
)

# Create executable
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Cinder
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${PROJECT_RESOURCES}
    )
else()
    add_executable(Cinder
        ${PROJECT_SOURCES}
        ${PROJECT_RESOURCES}
    )
endif()

# Include paths — allows #include "Theme.h" instead of "../../Theme.h"
target_include_directories(Cinder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${SODIUM_INCLUDE_DIRS}
    ${QRENCODE_INCLUDE_DIRS}
    ${HIDAPI_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIRS}
)

# Link Qt libraries
target_link_libraries(Cinder
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::PrintSupport
    ${QT_SVG_LIBRARY}
    ${SODIUM_LIBRARIES}
    ${QRENCODE_LIBRARIES}
    ${HIDAPI_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    ZLIB::ZLIB
)

# Platform-specific settings
if(WIN32)
    set_target_properties(Cinder PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE app-icon.icns)
    set_target_properties(Cinder PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE ${MACOSX_BUNDLE_ICON_FILE}
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.cinderwallet.desktop"
        MACOSX_BUNDLE_BUNDLE_NAME "Cinder"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
    target_link_libraries(Cinder PRIVATE
        "-framework Cocoa"
        "-framework LocalAuthentication"
        "-framework Security"
    )

    # Copy icons into app bundle Resources/ so native macOS code can access them
    set(MACOS_BUNDLE_RESOURCES
        resources/titlebar-icon.png
        resources/app-icon.icns
    )
    set_source_files_properties(${MACOS_BUNDLE_RESOURCES} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(Cinder PRIVATE ${MACOS_BUNDLE_RESOURCES})

    # Code sign with Developer ID for stable Keychain identity + biometric access.
    # Keep the default aligned with scripts/build-dmg.sh and skip local app signing
    # when the requested identity is not currently available in the login keychain.
    option(CINDER_ENABLE_APP_SIGNING "Code sign the macOS app bundle after build" ON)
    set(CINDER_CODESIGN_DEFAULT_IDENTITY "$ENV{CINDER_SIGNING_IDENTITY}")
    if(NOT CINDER_CODESIGN_DEFAULT_IDENTITY)
        set(CINDER_CODESIGN_DEFAULT_IDENTITY "$ENV{CINDER_CODESIGN_IDENTITY}")
    endif()
    set(CINDER_CODESIGN_IDENTITY "${CINDER_CODESIGN_DEFAULT_IDENTITY}" CACHE STRING
        "Code signing identity used for macOS app signing")
    set(CINDER_CODESIGN_ENTITLEMENTS "")
    if(CINDER_TEAM_ID)
        set(CINDER_CODESIGN_ENTITLEMENTS
            "${CMAKE_CURRENT_BINARY_DIR}/generated/Cinder.entitlements")
    endif()

    if(CINDER_ENABLE_APP_SIGNING)
        if(CINDER_CODESIGN_IDENTITY)
            find_program(SECURITY_EXECUTABLE security)
            if(SECURITY_EXECUTABLE)
                execute_process(
                    COMMAND ${SECURITY_EXECUTABLE} find-identity -v -p codesigning
                    OUTPUT_VARIABLE CINDER_CODESIGN_IDENTITIES
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                )
            endif()

            string(FIND "${CINDER_CODESIGN_IDENTITIES}" "${CINDER_CODESIGN_IDENTITY}"
                CINDER_CODESIGN_IDENTITY_INDEX)
        else()
            set(CINDER_CODESIGN_IDENTITY_INDEX -1)
        endif()
        if(CINDER_CODESIGN_IDENTITY_INDEX GREATER -1)
            if(CINDER_CODESIGN_ENTITLEMENTS)
                add_custom_command(TARGET Cinder POST_BUILD
                    COMMAND codesign --force --sign "${CINDER_CODESIGN_IDENTITY}"
                            --entitlements "${CINDER_CODESIGN_ENTITLEMENTS}"
                            "$<TARGET_BUNDLE_DIR:Cinder>"
                    COMMENT "Signing Cinder.app with Developer ID certificate"
                    VERBATIM
                )
            else()
                add_custom_command(TARGET Cinder POST_BUILD
                    COMMAND codesign --force --sign "${CINDER_CODESIGN_IDENTITY}"
                            "$<TARGET_BUNDLE_DIR:Cinder>"
                    COMMENT "Signing Cinder.app with Developer ID certificate"
                    VERBATIM
                )
            endif()
        else()
            add_custom_command(TARGET Cinder POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E rm -rf
                        "$<TARGET_BUNDLE_DIR:Cinder>/Contents/_CodeSignature"
                COMMENT "Removing stale Cinder.app signature metadata"
                VERBATIM
            )
            message(WARNING
                "Skipping Cinder.app codesign: requested identity "
                "'${CINDER_CODESIGN_IDENTITY}' is not available. "
                "Release packaging can still sign via scripts/build-dmg.sh.")
        endif()
    endif()
endif()

# Finalize executable (Qt6 only)
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Cinder)
endif()

# i18n: extract tr() strings, compile .qm, embed in binary
if(QT_VERSION_MAJOR EQUAL 6 AND Qt6LinguistTools_FOUND)
    set(TS_FILES
        translations/Cinder_en.ts
        translations/Cinder_es.ts
        translations/Cinder_zh_CN.ts
        translations/Cinder_ja.ts
        translations/Cinder_ko.ts
        translations/Cinder_pt_BR.ts
        translations/Cinder_fr.ts
        translations/Cinder_de.ts
    )

    qt_add_translations(Cinder
        TS_FILES ${TS_FILES}
        RESOURCE_PREFIX "/i18n"
        LUPDATE_OPTIONS -no-obsolete
    )
endif()

# ── Tests (Google Test) ────────────────────────────────
enable_testing()
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
# Prevent gtest from overriding compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── Standalone MCP server binary ──────────────────────────────────
add_executable(cinder-mcp
    src/agents/cinder-mcp-main.cpp
    src/agents/McpRpcHelper.cpp
    src/agents/McpRpcHelper.h
    src/agents/McpServer.cpp
    src/agents/McpServer.h
    src/agents/McpToolDefs.h
    src/db/Database.cpp
    src/db/Database.h
    src/db/DbUtil.cpp
    src/db/DbUtil.h
    src/db/McpDb.cpp
    src/db/McpDb.h
    src/db/ContactDb.cpp
    src/db/ContactDb.h
    src/db/NonceDb.cpp
    src/db/NonceDb.h
    src/db/TokenAccountDb.cpp
    src/db/TokenAccountDb.h
    src/db/PortfolioDb.cpp
    src/db/PortfolioDb.h
    src/db/TransactionDb.cpp
    src/db/TransactionDb.h
    src/db/ValidatorCacheDb.cpp
    src/db/ValidatorCacheDb.h
    src/db/WalletDb.cpp
    src/db/WalletDb.h
    src/tx/TxParseUtils.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(cinder-mcp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(cinder-mcp PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
)

# Copy cinder-mcp into the app bundle so the wallet can find it
add_custom_command(TARGET cinder-mcp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:cinder-mcp>
        $<TARGET_FILE_DIR:Cinder>/cinder-mcp
    COMMENT "Copying cinder-mcp into app bundle"
)

add_executable(test_database
    tests/test_database.cpp
    src/db/Database.cpp
    src/db/DbUtil.cpp
    src/db/DbUtil.h
    src/db/TransactionDb.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_database PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_database PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
    GTest::gtest
)
add_executable(test_solana_api
    tests/test_solana_api.cpp
    src/services/SolanaApi.cpp
)
target_include_directories(test_solana_api PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_solana_api PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
    GTest::gtest
)

add_executable(test_sleep_wake
    tests/test_sleep_wake.cpp
    src/services/SolanaApi.cpp
    src/services/PriceService.cpp
    src/db/TokenAccountDb.cpp
    src/db/Database.cpp
    src/db/DbUtil.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_sleep_wake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_sleep_wake PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Test
    GTest::gtest
)

add_executable(test_create_token_sim
    tests/test_create_token_sim.cpp
    src/services/SolanaApi.cpp
    src/tx/TransactionBuilder.cpp
    src/tx/AssociatedTokenInstruction.cpp
    src/tx/TokenOperationBuilder.cpp
    src/crypto/Keypair.cpp
    src/features/sendreceive/SendReceiveHandler.cpp
    src/db/TokenAccountDb.cpp
    src/db/PortfolioDb.cpp
    src/db/ContactDb.cpp
    src/db/Database.cpp
    src/db/DbUtil.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_create_token_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SODIUM_INCLUDE_DIRS}
)
target_link_libraries(test_create_token_sim PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    ${SODIUM_LIBRARIES}
    GTest::gtest
)

add_executable(test_transaction_builder
    tests/test_transaction_builder.cpp
    src/tx/TransactionBuilder.cpp
    src/tx/AssociatedTokenInstruction.cpp
)
target_include_directories(test_transaction_builder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SODIUM_INCLUDE_DIRS}
)
target_link_libraries(test_transaction_builder PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    ${SODIUM_LIBRARIES}
    GTest::gtest
)

add_executable(test_transaction_executor
    tests/test_transaction_executor.cpp
    src/tx/TransactionBuilder.cpp
    src/tx/TransactionExecutor.cpp
    src/services/SolanaApi.cpp
    src/crypto/Keypair.cpp
)
target_include_directories(test_transaction_executor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SODIUM_INCLUDE_DIRS}
)
target_link_libraries(test_transaction_executor PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    ${SODIUM_LIBRARIES}
    GTest::gtest
)

add_executable(test_crypto
    tests/test_crypto.cpp
    src/crypto/Keypair.cpp
    src/crypto/Mnemonic.cpp
)
target_include_directories(test_crypto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SODIUM_INCLUDE_DIRS}
)
target_link_libraries(test_crypto PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    ${SODIUM_LIBRARIES}
    GTest::gtest
)

add_executable(test_token2022
    tests/test_token2022.cpp
    src/tx/TransactionBuilder.cpp
)
target_include_directories(test_token2022 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_token2022 PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    GTest::gtest
)

add_executable(test_token_dropdown
    tests/test_token_dropdown.cpp
    src/widgets/TokenDropdown.cpp
    src/services/AvatarCache.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_token_dropdown PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_token_dropdown PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
    GTest::gtest
)

add_executable(test_wallet_restore
    tests/test_wallet_restore.cpp
)
target_include_directories(test_wallet_restore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_wallet_restore PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    GTest::gtest
)

add_executable(test_contact_db
    tests/test_contact_db.cpp
    src/db/Database.cpp
    src/db/DbUtil.cpp
    src/db/DbUtil.h
    src/db/ContactDb.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_contact_db PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_contact_db PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
    GTest::gtest
)

add_executable(test_token_account_db
    tests/test_token_account_db.cpp
    src/db/Database.cpp
    src/db/DbUtil.cpp
    src/db/DbUtil.h
    src/db/TokenAccountDb.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_token_account_db PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_token_account_db PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
    GTest::gtest
)

add_executable(test_mcp_db
    tests/test_mcp_db.cpp
    src/db/Database.cpp
    src/db/DbUtil.cpp
    src/db/DbUtil.h
    src/db/McpDb.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_mcp_db PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_mcp_db PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
    GTest::gtest
)

add_executable(test_instruction_decoder
    tests/test_instruction_decoder.cpp
    src/services/IdlRegistry.cpp
    src/services/SolanaApi.cpp
    src/db/IdlDb.cpp
    src/db/Database.cpp
    src/db/DbUtil.cpp
    src/db/DbUtil.h
    ${PROJECT_RESOURCES}
)
target_include_directories(test_instruction_decoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SODIUM_INCLUDE_DIRS}
)
target_link_libraries(test_instruction_decoder PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
    ${SODIUM_LIBRARIES}
    ZLIB::ZLIB
    GTest::gtest
)

add_executable(test_known_tokens
    tests/test_known_tokens.cpp
)
target_include_directories(test_known_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_known_tokens PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    GTest::gtest
)

add_executable(test_theme_visual_pivot
    tests/test_theme_visual_pivot.cpp
    src/features/assets/AssetsPage.cpp
    src/features/assets/AssetsHandler.cpp
    src/models/Assets.h
    src/widgets/AddressLink.cpp
    src/widgets/SplineChart.cpp
    src/widgets/QrCodeWidget.cpp
    src/widgets/TokenDropdown.cpp
    src/widgets/Dropdown.cpp
    src/db/Database.cpp
    src/db/DbUtil.cpp
    src/db/DbUtil.h
    src/db/TokenAccountDb.cpp
    src/db/PortfolioDb.cpp
    src/services/AvatarCache.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_theme_visual_pivot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${QRENCODE_INCLUDE_DIRS}
)
target_link_libraries(test_theme_visual_pivot PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Test
    ${QRENCODE_LIBRARIES}
    ZLIB::ZLIB
    GTest::gtest
)

add_executable(test_terminal_command_catalog
    tests/test_terminal_command_catalog.cpp
    tests/test_terminal_completion.cpp
    src/features/terminal/TerminalCompletion.cpp
    src/features/terminal/TerminalCommandCatalog.cpp
)
target_include_directories(test_terminal_command_catalog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_terminal_command_catalog PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)

# UI tests use offscreen platform so they don't steal focus or show windows
set(OFFSCREEN_ENV "QT_QPA_PLATFORM=offscreen")

gtest_discover_tests(test_known_tokens)
gtest_discover_tests(test_terminal_command_catalog)
gtest_discover_tests(test_theme_visual_pivot)
gtest_discover_tests(test_instruction_decoder)
gtest_discover_tests(test_token_account_db)
gtest_discover_tests(test_contact_db)
gtest_discover_tests(test_mcp_db)
gtest_discover_tests(test_wallet_restore)
gtest_discover_tests(test_database)
gtest_discover_tests(test_solana_api)
gtest_discover_tests(test_sleep_wake)
gtest_discover_tests(test_transaction_builder)
gtest_discover_tests(test_transaction_executor)
gtest_discover_tests(test_crypto)
gtest_discover_tests(test_token2022)
gtest_discover_tests(test_token_dropdown PROPERTIES ENVIRONMENT "${OFFSCREEN_ENV}")

add_executable(test_tx_parsing
    tests/test_tx_parsing.cpp
    src/db/Database.cpp
    src/db/DbUtil.cpp
    src/db/DbUtil.h
    src/db/TransactionDb.cpp
    src/tx/TxParseUtils.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_tx_parsing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_tx_parsing PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
    GTest::gtest
)
gtest_discover_tests(test_tx_parsing)

add_executable(test_stake_lifecycle
    tests/test_stake_lifecycle.cpp
)
target_include_directories(test_stake_lifecycle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_stake_lifecycle PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    GTest::gtest
    GTest::gtest_main
)
gtest_discover_tests(test_stake_lifecycle)

add_executable(test_sidebar_alignment
    tests/test_sidebar_alignment.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_sidebar_alignment PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_sidebar_alignment PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    GTest::gtest
)
gtest_discover_tests(test_sidebar_alignment PROPERTIES ENVIRONMENT "${OFFSCREEN_ENV}")

add_executable(test_stress
    tests/test_stress.cpp
    # Pages under test
    src/features/dashboard/DashboardPage.cpp
    src/features/dashboard/DashboardHandler.cpp
    src/features/staking/StakingPage.cpp
    src/features/staking/StakeStatusRail.cpp
    src/features/staking/StakingHandler.cpp
    src/models/Staking.h
    src/features/activity/ActivityPage.cpp
    src/features/activity/ActivityHandler.cpp
    src/models/Activity.h
    src/features/assets/AssetsPage.cpp
    src/features/assets/AssetsHandler.cpp
    src/models/Assets.h
    src/features/addressbook/AddressBookPage.cpp
    src/features/addressbook/AddressBookHandler.cpp
    src/models/AddressBook.h
    src/features/wallets/WalletsPage.cpp
    src/features/wallets/WalletsHandler.cpp
    src/features/swap/SwapPage.cpp
    src/features/swap/SwapHandler.cpp
    src/models/Swap.h
    src/features/txlookup/TxLookupPage.cpp
    src/features/txlookup/TxLookupHandler.cpp
    src/models/TxLookup.h
    # Widgets used by those pages
    src/widgets/SplineChart.cpp
    src/widgets/TpsBarChart.cpp
    src/widgets/CardListItem.cpp
    src/widgets/TokenDropdown.cpp
    src/widgets/Dropdown.cpp
    src/widgets/AddressLink.cpp
    src/widgets/AddressInput.cpp
    src/widgets/AmountInput.cpp
    src/widgets/UploadWidget.cpp
    src/widgets/ComputeUnitsBar.cpp
    src/widgets/FlapDisplay.cpp
    src/widgets/QrCodeWidget.cpp
    src/widgets/StyledCalendar.cpp
    src/widgets/PillButtonGroup.h
    src/widgets/TabBar.cpp
    src/widgets/ToggleSwitch.cpp
    src/widgets/TxStatusAnimationWidget.cpp
    # Utilities
    src/util/ContactResolver.cpp
    # Crypto (used by DashboardPage HW debug)
    src/crypto/Signer.h
    src/crypto/SignerManager.cpp
    # Services
    src/services/SolanaApi.cpp
    src/services/JupiterApi.cpp
    src/services/ValidatorService.cpp
    src/services/AvatarCache.cpp
    src/services/NetworkStatsService.cpp
    src/services/IdlRegistry.cpp
    src/services/PriceService.cpp
    src/services/model/JupiterQuote.h
    src/services/model/PriceData.h
    # Database
    src/db/Database.cpp
    src/db/DbUtil.cpp
    src/db/DbUtil.h
    src/db/TransactionDb.cpp
    src/db/TokenAccountDb.cpp
    src/db/PortfolioDb.cpp
    src/db/NetworkStatsDb.cpp
    src/db/ValidatorCacheDb.cpp
    src/db/StakeAccountDb.cpp
    src/db/StakeAccountRewardDb.cpp
    src/db/ContactDb.cpp
    src/db/SyncStateDb.cpp
    src/db/NotificationDb.cpp
    src/db/WalletDb.cpp
    src/db/IdlDb.cpp
    # Transaction builder (used by StakingPage)
    src/tx/TransactionBuilder.cpp
    src/tx/TransactionExecutor.cpp
    src/tx/SystemOperationBuilder.cpp
    src/tx/InstructionDecoder.h
    src/tx/TxParseUtils.cpp
    src/tx/TxClassifier.h
    # Crypto (used by StakingPage keypair)
    src/crypto/Keypair.cpp
    src/crypto/WalletCrypto.cpp
    ${PROJECT_RESOURCES}
)
target_include_directories(test_stress PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${QRENCODE_INCLUDE_DIRS}
    ${SODIUM_INCLUDE_DIRS}
    ${HIDAPI_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIRS}
)
target_link_libraries(test_stress PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Test
    ${QRENCODE_LIBRARIES}
    ${SODIUM_LIBRARIES}
    ZLIB::ZLIB
    GTest::gtest
)
gtest_discover_tests(test_stress)

add_executable(test_feature_visual_full
    tests/test_feature_visual_full.cpp
    src/features/sendreceive/SendReceivePage.cpp
    src/features/sendreceive/SendReceiveForm.cpp
    src/features/sendreceive/SendReceiveReview.cpp
    src/features/sendreceive/SendReceiveReviewActions.cpp
    src/features/sendreceive/SendReceiveNonce.cpp
    src/features/sendreceive/SendReceiveCreateToken.cpp
    src/features/sendreceive/SendReceiveCreateTokenReview.cpp
    src/features/sendreceive/SendReceiveBurnToken.cpp
    src/features/sendreceive/SendReceiveCloseAccounts.cpp
    src/features/sendreceive/SendReceiveMintToken.cpp
    src/features/sendreceive/SendReceiveSuccess.cpp
    src/features/sendreceive/SendReceiveHandler.cpp
    src/features/sendreceive/SendReceiveHandlerCreateTokenFlow.cpp
    src/features/sendreceive/SendReceiveHandlerFlow.cpp
    src/features/sendreceive/CreateTokenHandler.cpp
    src/features/sendreceive/MintTokenHandler.cpp
    src/features/sendreceive/BurnTokenHandler.cpp
    src/features/sendreceive/CloseTokenAccountsHandler.cpp
    src/features/terminal/TerminalPage.cpp
    src/features/terminal/TerminalHandler.cpp
    src/features/terminal/TerminalHandlerData.cpp
    src/features/terminal/TerminalHandlerRpcNonce.cpp
    src/features/terminal/TerminalHandlerStakeValidator.cpp
    src/features/terminal/TerminalHandlerSwapPrice.cpp
    src/features/terminal/TerminalAsyncOp.cpp
    src/features/terminal/TerminalCommandContext.cpp
    src/features/terminal/TerminalHandlerTx.cpp
    src/features/terminal/TerminalHandlerTxRender.cpp
    src/features/terminal/TerminalHandlerTokenAccount.cpp
    src/features/terminal/TerminalHandlerNetwork.cpp
    src/features/terminal/TerminalRequestTracker.cpp
    src/features/terminal/TerminalSecretFlow.cpp
    src/features/terminal/TerminalCompletion.cpp
    src/features/terminal/TerminalCommandCatalog.cpp
    src/features/settings/SettingsPage.cpp
    src/features/settings/SettingsHandler.cpp
    src/features/agents/AgentsPage.cpp
    src/features/agents/AgentsHandler.cpp
    src/features/agents/ApprovalExecutor.cpp
    src/features/lockscreen/LockScreen.cpp
    src/features/lockscreen/LockScreenHandler.cpp
    src/features/txlookup/TxLookupPage.cpp
    src/features/txlookup/TxLookupHandler.cpp
    src/models/SendReceive.h
    src/models/Agents.h
    src/widgets/AddressInput.cpp
    src/widgets/AmountInput.cpp
    src/widgets/AddressLink.cpp
    src/widgets/TokenDropdown.cpp
    src/widgets/Dropdown.cpp
    src/widgets/UploadWidget.cpp
    src/widgets/ComputeUnitsBar.cpp
    src/widgets/StyledCheckbox.h
    src/widgets/PillButtonGroup.cpp
    src/widgets/TabBar.cpp
    src/widgets/SplineChart.cpp
    src/widgets/QrCodeWidget.cpp
    src/widgets/TxStatusAnimationWidget.cpp
    src/services/SolanaApi.cpp
    src/services/JupiterApi.cpp
    src/services/AvatarCache.cpp
    src/services/ValidatorService.cpp
    src/services/NetworkStatsService.cpp
    src/services/PriceService.cpp
    src/services/IdlRegistry.cpp
    src/services/IrysApi.cpp
    src/db/Database.cpp
    src/db/DbUtil.cpp
    src/db/DbUtil.h
    src/db/WalletDb.cpp
    src/db/TransactionDb.cpp
    src/db/ContactDb.cpp
    src/db/TokenAccountDb.cpp
    src/db/PortfolioDb.cpp
    src/db/IdlDb.cpp
    src/db/NonceDb.cpp
    src/db/ValidatorCacheDb.cpp
    src/db/NetworkStatsDb.cpp
    src/db/SyncStateDb.cpp
    src/db/NotificationDb.cpp
    src/db/McpDb.cpp
    src/agents/McpClient.cpp
    src/agents/ModelProvider.cpp
    src/agents/models/codex/CodexProvider.cpp
    src/agents/models/claude/ClaudeProvider.cpp
    src/agents/models/claudecodecli/ClaudeCodeCliProvider.cpp
    src/crypto/Keypair.cpp
    src/crypto/Signer.cpp
    src/crypto/WalletCrypto.cpp
    src/crypto/Mnemonic.cpp
    src/crypto/SoftwareSigner.cpp
    src/tx/TransactionBuilder.cpp
    src/tx/TransactionExecutor.cpp
    src/tx/SystemOperationBuilder.cpp
    src/tx/AssociatedTokenInstruction.cpp
    src/tx/TxParseUtils.cpp
    src/tx/TokenOperationBuilder.cpp
    src/util/ContactResolver.cpp
    src/util/MacUtils.mm
    src/util/MacUtils.h
    ${PROJECT_RESOURCES}
)
target_include_directories(test_feature_visual_full PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${QRENCODE_INCLUDE_DIRS}
    ${SODIUM_INCLUDE_DIRS}
    ${HIDAPI_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIRS}
)
target_link_libraries(test_feature_visual_full PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Test
    ${QRENCODE_LIBRARIES}
    ${SODIUM_LIBRARIES}
    ${HIDAPI_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    ZLIB::ZLIB
    GTest::gtest
)
if(APPLE)
    target_link_libraries(test_feature_visual_full PRIVATE
        "-framework Cocoa"
        "-framework LocalAuthentication"
        "-framework Security"
    )
endif()
gtest_discover_tests(test_feature_visual_full)

add_executable(test_trezor_protobuf
    tests/test_trezor_protobuf.cpp
    src/crypto/plugin/trezor/TrezorProtobuf.cpp
    src/crypto/plugin/trezor/TrezorTransport.cpp
)
target_include_directories(test_trezor_protobuf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_trezor_protobuf PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    GTest::gtest
)
gtest_discover_tests(test_trezor_protobuf)

add_executable(test_features_decode
    tests/test_features_decode.cpp
    src/crypto/plugin/trezor/TrezorProtobuf.cpp
    src/crypto/plugin/trezor/TrezorTransport.cpp
)
target_include_directories(test_features_decode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_features_decode PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    GTest::gtest
)
gtest_discover_tests(test_features_decode)

add_executable(test_trezor_wire_framing
    tests/test_trezor_wire_framing.cpp
    src/crypto/plugin/trezor/TrezorProtobuf.cpp
    src/crypto/plugin/trezor/TrezorTransport.cpp
)
target_include_directories(test_trezor_wire_framing PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(test_trezor_wire_framing PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    GTest::gtest
)
gtest_discover_tests(test_trezor_wire_framing)

add_executable(test_trezor_usb_context
    tests/test_trezor_usb_context.cpp
    src/crypto/plugin/trezor/TrezorUsbTransport.cpp
    src/crypto/plugin/trezor/TrezorProtobuf.cpp
    src/crypto/plugin/trezor/TrezorTransport.cpp
)
target_include_directories(test_trezor_usb_context PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBUSB_INCLUDE_DIRS}
)
target_link_libraries(test_trezor_usb_context PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    GTest::gtest
    GTest::gtest_main
    ${LIBUSB_LIBRARIES}
)
gtest_discover_tests(test_trezor_usb_context)

add_executable(test_ledger_transport
    tests/test_ledger_transport.cpp
    src/crypto/plugin/ledger/LedgerTransport.cpp
    src/crypto/plugin/ledger/LedgerPlugin.cpp
    src/crypto/plugin/ledger/LedgerSigner.cpp
    src/crypto/Signer.h
    src/crypto/HardwareWalletPlugin.h
)
target_include_directories(test_ledger_transport PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(test_ledger_transport PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    GTest::gtest
    ${HIDAPI_LIBRARIES}
)
target_include_directories(test_ledger_transport PRIVATE ${HIDAPI_INCLUDE_DIRS})
gtest_discover_tests(test_ledger_transport)

# Install targets
install(TARGETS Cinder
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
