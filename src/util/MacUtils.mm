#include "MacUtils.h"
#import <Cocoa/Cocoa.h>
#import <LocalAuthentication/LocalAuthentication.h>
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QObject>
#import <Security/Security.h>
#include <functional>

// Flipped NSView so Y=0 is top — matches toolbar coordinate system
@interface FlippedView : NSView
@end
@implementation FlippedView
- (BOOL)isFlipped {
    return YES;
}
@end

// NSButton subclass with hover effect and pointing hand cursor
@interface HoverButton : NSButton
@property(nonatomic) BOOL isHovered;
@end

@implementation HoverButton

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    for (NSTrackingArea* area in self.trackingAreas) {
        [self removeTrackingArea:area];
    }
    NSTrackingArea* trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways)
               owner:self
            userInfo:nil];
    [self addTrackingArea:trackingArea];
}

- (void)mouseEntered:(NSEvent*)event {
    self.isHovered = YES;
    self.layer.backgroundColor = [[NSColor colorWithWhite:1.0 alpha:0.12] CGColor];
    self.layer.cornerRadius = 8;
    self.contentTintColor = [NSColor whiteColor];
}

- (void)mouseExited:(NSEvent*)event {
    self.isHovered = NO;
    self.layer.backgroundColor = [NSColor clearColor].CGColor;
    self.contentTintColor = [NSColor colorWithWhite:0.85 alpha:1.0];
}

- (void)resetCursorRects {
    [self addCursorRect:self.bounds cursor:[NSCursor pointingHandCursor]];
}

// Prevent default highlight flash on click
- (void)highlight:(BOOL)flag {
    // no-op: we handle visual states ourselves via hover
}

@end

// Static references
static NSView* bellContainer = nil;
static NSView* currentBadge = nil;
static HoverButton* bellButton = nil;
static HoverButton* sidebarToggleButton = nil;
static NSToolbarItem* sidebarToggleItem = nil;
static NSToolbarItem* bellItem = nil;
static NSView* connectionContainer = nil;
static NSTextField* connectionLabel = nil;
static bool toolbarItemsVisible = true;
static std::function<void()> sidebarToggleCallback;
static std::function<void()> bellClickCallback;

// Sidebar toggle button target
@interface SidebarToggleTarget : NSObject
- (void)toggleSidebar:(id)sender;
@end

@implementation SidebarToggleTarget
- (void)toggleSidebar:(id)sender {
    if (sidebarToggleCallback) {
        sidebarToggleCallback();
    }
}
@end

static SidebarToggleTarget* toggleTarget = nil;

// Bell button target
@interface BellButtonTarget : NSObject
- (void)onBellClicked:(id)sender;
@end

@implementation BellButtonTarget
- (void)onBellClicked:(id)sender {
    if (bellClickCallback) {
        bellClickCallback();
    }
}
@end

static BellButtonTarget* bellTarget = nil;

// Toolbar delegate
@interface ToolbarDelegate : NSObject <NSToolbarDelegate>
@end

@implementation ToolbarDelegate

- (NSArray<NSToolbarItemIdentifier>*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar {
    return @[
        @"sidebarToggle", NSToolbarFlexibleSpaceItemIdentifier, @"connectionStatus",
        @"notificationBell"
    ];
}

- (NSArray<NSToolbarItemIdentifier>*)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar {
    return @[
        @"sidebarToggle", NSToolbarFlexibleSpaceItemIdentifier, @"connectionStatus",
        @"notificationBell"
    ];
}

- (NSToolbarItem*)toolbar:(NSToolbar*)toolbar
        itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier
    willBeInsertedIntoToolbar:(BOOL)flag {

    if ([itemIdentifier isEqualToString:@"sidebarToggle"]) {
        NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

        // Container adds left padding so icon isn't flush against window edge
        NSView* container = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 42, 30)];
        container.wantsLayer = YES;

        // Use HoverButton for consistent hover effect with bell
        HoverButton* button = [[HoverButton alloc] initWithFrame:NSMakeRect(8, -4, 30, 30)];
        button.bezelStyle = NSBezelStyleInline;
        button.bordered = NO;
        button.title = @"";

        // Load icon from bundle, set as template for tinting
        NSString* iconPath = [[NSBundle mainBundle] pathForResource:@"titlebar-icon" ofType:@"png"];
        NSImage* iconImage = iconPath ? [[NSImage alloc] initWithContentsOfFile:iconPath] : nil;

        if (!iconImage) {
            iconImage = [NSImage imageWithSystemSymbolName:@"sidebar.left"
                                  accessibilityDescription:@"Toggle Sidebar"];
        }

        [iconImage setTemplate:YES];
        iconImage.size = NSMakeSize(16, 16);
        button.image = iconImage;
        button.contentTintColor = [NSColor colorWithWhite:0.85 alpha:1.0];
        button.wantsLayer = YES;
        button.layer.backgroundColor = [NSColor clearColor].CGColor;

        // Connect click
        if (!toggleTarget) {
            toggleTarget = [[SidebarToggleTarget alloc] init];
        }
        button.target = toggleTarget;
        button.action = @selector(toggleSidebar:);
        sidebarToggleButton = button;

        [container addSubview:button];
        container.hidden = !toolbarItemsVisible;
        item.view = container;
        sidebarToggleItem = item;
        return item;
    }

    if ([itemIdentifier isEqualToString:@"connectionStatus"]) {
        NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

        // Rounded-rect badge with text inside (like the RECEIVE badge)
        CGFloat badgeH = 28;
        CGFloat padX = 14;

        // Label — size for widest text "NOT CONNECTED"
        NSTextField* label = [NSTextField labelWithString:@"NOT CONNECTED"];
        label.font = [NSFont systemFontOfSize:11 weight:NSFontWeightBold];
        label.textColor = [NSColor colorWithRed:0.937 green:0.267 blue:0.267 alpha:1.0];
        label.backgroundColor = [NSColor clearColor];
        label.drawsBackground = NO;
        label.alignment = NSTextAlignmentCenter;
        [label sizeToFit];
        CGFloat labelW = label.frame.size.width;
        CGFloat labelH = label.frame.size.height;
        CGFloat badgeW = labelW + padX * 2;

        // Badge container — FlippedView so Y=0 is top, standard centering works
        FlippedView* badge = [[FlippedView alloc] initWithFrame:NSMakeRect(0, 0, badgeW, badgeH)];
        badge.wantsLayer = YES;
        badge.layer.backgroundColor =
            [NSColor colorWithRed:0.20 green:0.08 blue:0.08 alpha:1.0].CGColor; // dark red
        badge.layer.borderColor =
            [NSColor colorWithRed:0.937 green:0.267 blue:0.267 alpha:0.4].CGColor;
        badge.layer.borderWidth = 1.0;
        badge.layer.cornerRadius = 6.0;
        connectionContainer = badge;

        // Center label inside badge
        label.frame = NSMakeRect(padX, round((badgeH - labelH) / 2.0) - 3, labelW, labelH);
        connectionLabel = label;

        [badge addSubview:label];
        badge.hidden = YES; // shown on first RPC response

        item.view = badge;
        return item;
    }

    if ([itemIdentifier isEqualToString:@"notificationBell"]) {
        NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

        // Container view to hold button + badge
        NSView* container = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 44, 30)];
        container.wantsLayer = YES;
        bellContainer = container;

        // Bell button with hover effect and pointing hand cursor
        HoverButton* button = [[HoverButton alloc] initWithFrame:NSMakeRect(0, -4, 30, 30)];
        button.bezelStyle = NSBezelStyleInline;
        button.bordered = NO;
        button.title = @"";
        button.image = [NSImage imageWithSystemSymbolName:@"bell.fill"
                                 accessibilityDescription:@"Notifications"];
        button.contentTintColor = [NSColor colorWithWhite:0.85 alpha:1.0];
        button.wantsLayer = YES;
        button.layer.backgroundColor = [NSColor clearColor].CGColor;
        bellButton = button;

        // Connect click
        if (!bellTarget) {
            bellTarget = [[BellButtonTarget alloc] init];
        }
        bellButton.target = bellTarget;
        bellButton.action = @selector(onBellClicked:);

        [container addSubview:button];
        container.hidden = !toolbarItemsVisible;

        item.view = container;
        bellItem = item;
        return item;
    }
    return nil;
}

@end

// Keep a strong reference so the delegate isn't deallocated
static ToolbarDelegate* toolbarDelegateInstance = nil;

namespace {
    bool keychainItemExists(NSString* service, NSString* account) {
        NSDictionary* query = @{
            (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService : service,
            (__bridge id)kSecAttrAccount : account,
            (__bridge id)kSecReturnAttributes : @YES,
            (__bridge id)kSecMatchLimit : (__bridge id)kSecMatchLimitOne,
        };
        CFTypeRef result = nullptr;
        const OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
        if (result) {
            CFRelease(result);
        }
        return status == errSecSuccess;
    }

    OSStatus addGenericPasswordItem(NSString* service, NSString* account, NSData* passwordData) {
        NSDictionary* addQuery = @{
            (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService : service,
            (__bridge id)kSecAttrAccount : account,
            (__bridge id)kSecValueData : passwordData,
            (__bridge id)kSecAttrAccessible : (__bridge id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
        };
        return SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
    }
}

void setupTransparentTitleBar(QWindow* qtWindow) {
    if (!qtWindow) {
        return;
    }

    NSView* view = reinterpret_cast<NSView*>(qtWindow->winId());
    NSWindow* window = [view window];

    // Transparent title bar with hidden title - shows window background color
    window.titlebarAppearsTransparent = YES;
    window.titleVisibility = NSWindowTitleHidden;

    // Toolbar with sidebar toggle on left, bell on right
    if (!toolbarDelegateInstance) {
        toolbarDelegateInstance = [[ToolbarDelegate alloc] init];
    }

    NSToolbar* toolbar = [[NSToolbar alloc] initWithIdentifier:@"mainToolbar"];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    toolbar.showsBaselineSeparator = NO;
#pragma clang diagnostic pop
    toolbar.delegate = toolbarDelegateInstance;
    window.toolbar = toolbar;

    // Background matches sidebar top gradient color (#1a1b2e) so title bar blends
    window.backgroundColor = [NSColor colorWithRed:0.102 green:0.106 blue:0.180 alpha:1.0];

    // Centered title label overlaid on the titlebar (stays centered in full window)
    NSView* themeFrame = window.contentView.superview;
    if (themeFrame) {
        // Remove any previously added title label (re-entry after fullscreen restore)
        for (NSView* sub in themeFrame.subviews) {
            if (sub.tag == 999) {
                [sub removeFromSuperview];
                break;
            }
        }

        NSString* titleText = [NSString
            stringWithFormat:@"Cinder — %@", QObject::tr("Desktop Solana Wallet").toNSString()];
        NSTextField* titleLabel = [NSTextField labelWithString:titleText];
        titleLabel.font = [NSFont boldSystemFontOfSize:13];
        titleLabel.textColor = [NSColor whiteColor];
        titleLabel.alignment = NSTextAlignmentCenter;
        titleLabel.backgroundColor = [NSColor clearColor];
        titleLabel.drawsBackground = NO;
        titleLabel.tag = 999;
        [titleLabel sizeToFit];

        // Position centered in titlebar (toolbar height is ~38pt)
        CGFloat titleBarHeight = 38.0;
        CGFloat frameW = themeFrame.frame.size.width;
        CGFloat frameH = themeFrame.frame.size.height;
        CGFloat labelW = titleLabel.frame.size.width;
        CGFloat labelH = titleLabel.frame.size.height;

        titleLabel.frame =
            NSMakeRect((frameW - labelW) / 2.0,
                       frameH - titleBarHeight + (titleBarHeight - labelH) / 2.0, labelW, labelH);

        titleLabel.autoresizingMask = (NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin);

        [themeFrame addSubview:titleLabel];
    }
}

void setSidebarToggleCallback(std::function<void()> callback) { sidebarToggleCallback = callback; }

void setBellClickCallback(std::function<void()> callback) { bellClickCallback = callback; }

void updateConnectionStatus(bool connected, const QString& text) {
    if (!connectionContainer) {
        return;
    }

    NSColor* textColor;
    NSColor* bgColor;
    NSColor* borderColor;
    connectionLabel.stringValue = text.toNSString();
    if (connected) {
        textColor = [NSColor colorWithRed:0.063 green:0.725 blue:0.506 alpha:1.0]; // #10b981
        bgColor = [NSColor colorWithRed:0.08 green:0.18 blue:0.16 alpha:1.0];      // dark teal
        borderColor = [NSColor colorWithRed:0.063 green:0.725 blue:0.506 alpha:0.4];
    } else {
        textColor = [NSColor colorWithRed:0.937 green:0.267 blue:0.267 alpha:1.0]; // #ef4444
        bgColor = [NSColor colorWithRed:0.20 green:0.08 blue:0.08 alpha:1.0];      // dark red
        borderColor = [NSColor colorWithRed:0.937 green:0.267 blue:0.267 alpha:0.4];
    }

    connectionLabel.textColor = textColor;
    connectionContainer.layer.backgroundColor = bgColor.CGColor;
    connectionContainer.layer.borderColor = borderColor.CGColor;

    // Resize badge to fit text
    [connectionLabel sizeToFit];
    CGFloat padX = 14;
    CGFloat badgeH = 28; // fixed height — don't read from frame (toolbar may resize it)
    CGFloat labelW = connectionLabel.frame.size.width;
    CGFloat labelH = connectionLabel.frame.size.height;
    CGFloat badgeW = labelW + padX * 2;
    connectionContainer.frame = NSMakeRect(connectionContainer.frame.origin.x,
                                           connectionContainer.frame.origin.y, badgeW, badgeH);
    // Center label in flipped container
    connectionLabel.frame = NSMakeRect(padX, round((badgeH - labelH) / 2.0) - 3, labelW, labelH);
    if (toolbarItemsVisible) {
        connectionContainer.hidden = NO;
    }
}

void setToolbarItemsVisible(bool visible) {
    toolbarItemsVisible = visible;
    if (sidebarToggleItem.view) {
        sidebarToggleItem.view.hidden = !visible;
    }
    if (connectionContainer) {
        connectionContainer.hidden = !visible;
    }
    if (bellItem.view) {
        bellItem.view.hidden = !visible;
    }
}

void updateNotificationBadge(int count) {
    if (!bellContainer) {
        return;
    }

    // Remove existing badge if any
    if (currentBadge) {
        [currentBadge removeFromSuperview];
        currentBadge = nil;
    }

    if (count <= 0) {
        return;
    }

    // Cap at 999+
    NSString* badgeText;
    if (count > 999) {
        badgeText = @"999+";
    } else {
        badgeText = [NSString stringWithFormat:@"%d", count];
    }

    CGFloat badgeHeight = 14.0;
    CGFloat horizontalPadding = 6.0;
    CGFloat fontSize = 8.0;

    NSTextField* badgeLabel = [NSTextField labelWithString:badgeText];
    badgeLabel.font = [NSFont systemFontOfSize:fontSize weight:NSFontWeightBold];
    badgeLabel.textColor = [NSColor whiteColor];
    badgeLabel.alignment = NSTextAlignmentCenter;
    badgeLabel.backgroundColor = [NSColor clearColor];
    badgeLabel.drawsBackground = NO;
    badgeLabel.bezeled = NO;
    badgeLabel.editable = NO;
    [badgeLabel sizeToFit];

    CGFloat badgeWidth;
    if (count < 10) {
        badgeWidth = badgeHeight; // perfect circle for single digit
    } else {
        badgeWidth = fmax(badgeLabel.frame.size.width + horizontalPadding, badgeHeight);
    }

    // Position badge at top-right of bell button
    CGFloat badgeX = 16;
    CGFloat badgeY = 10;
    NSView* badge =
        [[NSView alloc] initWithFrame:NSMakeRect(badgeX, badgeY, badgeWidth, badgeHeight)];
    badge.wantsLayer = YES;
    badge.layer.backgroundColor = [[NSColor colorWithRed:0.85 green:0.12 blue:0.12
                                                   alpha:1.0] CGColor];
    badge.layer.cornerRadius = badgeHeight / 2.0;
    badge.layer.zPosition = 10;

    CGFloat labelH = badgeLabel.frame.size.height;
    badgeLabel.frame = NSMakeRect(0, round((badgeHeight - labelH) / 2.0), badgeWidth, labelH);
    [badge addSubview:badgeLabel];

    [bellContainer addSubview:badge positioned:NSWindowAbove relativeTo:bellButton];
    currentBadge = badge;
}

void updateSidebarToggleTooltip(const QString& tooltip) {
    if (sidebarToggleItem) {
        sidebarToggleItem.toolTip = tooltip.toNSString();
    }
}

void updateNotificationBellTooltip(const QString& tooltip) {
    if (bellItem) {
        bellItem.toolTip = tooltip.toNSString();
    }
}

// Disable cursor rect management on all non-main popup windows and
// force the pointing hand cursor.  Qt's cursor APIs are completely
// bypassed by macOS for popup-level windows.
void setPointingHandCursorForPopup() {
    // Debug: write diagnostic info to file
    NSMutableString* log = [NSMutableString string];
    [log appendFormat:@"=== setPointingHandCursorForPopup called ===\n"];
    [log appendFormat:@"Total windows: %lu\n", (unsigned long)[[NSApp windows] count]];

    NSWindow* mainWin = [NSApp mainWindow];
    [log appendFormat:@"Main window: %@ level: %ld\n", mainWin, (long)mainWin.level];

    for (NSWindow* window in [NSApp windows]) {
        if (window.isVisible && window != mainWin) {
            // Downgrade from NSPopUpMenuWindowLevel (101) to
            // NSFloatingWindowLevel (3) — macOS enforces arrow cursor
            // for popup-menu-level windows regardless of what we set.
            [window setLevel:NSFloatingWindowLevel];
            [window disableCursorRects];
        }
    }
    [[NSCursor pointingHandCursor] set];
}

void restoreDefaultCursorForPopup() {
    [NSCursor pop];
    NSWindow* mainWin = [NSApp mainWindow];
    if (mainWin) {
        [mainWin enableCursorRects];
    }
}

// ── Biometric (Touch ID) + Keychain ─────────────────────────────

static NSString* const kKeychainService = @"com.cinder.wallet.biometric.v2";
static NSString* const kLegacyKeychainService = @"com.cinder.wallet";

namespace {

    OSStatus copyKeychainPassword(NSString* service, NSString* account, LAContext* context,
                                  QString& outPassword) {
        NSDictionary* query = @{
            (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService : service,
            (__bridge id)kSecAttrAccount : account,
            (__bridge id)kSecReturnData : @YES,
            (__bridge id)kSecUseAuthenticationContext : context,
        };

        CFTypeRef result = NULL;
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
        if (status == errSecSuccess && result) {
            NSData* data = (__bridge NSData*)result;
            NSString* pw = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            CFRelease(result);
            outPassword = QString::fromNSString(pw);
        } else if (result) {
            CFRelease(result);
        }
        return status;
    }

} // namespace

bool isBiometricAvailable() {
    LAContext* context = [[LAContext alloc] init];
    NSError* error = nil;
    BOOL available = [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                                          error:&error];
    return available == YES;
}

bool storeBiometricPassword(const QString& walletAddress, const QString& password) {
    NSString* account = walletAddress.toNSString();
    NSData* passwordData = [password.toNSString() dataUsingEncoding:NSUTF8StringEncoding];

    // Delete any existing entry first, including legacy insecure entries.
    NSDictionary* deleteQuery = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : kKeychainService,
        (__bridge id)kSecAttrAccount : account,
    };
    SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
    NSDictionary* legacyDeleteQuery = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : kLegacyKeychainService,
        (__bridge id)kSecAttrAccount : account,
    };
    SecItemDelete((__bridge CFDictionaryRef)legacyDeleteQuery);

    CFErrorRef error = NULL;
    SecAccessControlRef accessControl =
        SecAccessControlCreateWithFlags(NULL, kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
                                        kSecAccessControlBiometryCurrentSet, &error);
    if (!accessControl) {
        if (error) {
            CFRelease(error);
        }
        qWarning() << "storeBiometricPassword: failed to create biometric access control";
        return false;
    }

    NSDictionary* addQuery = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : kKeychainService,
        (__bridge id)kSecAttrAccount : account,
        (__bridge id)kSecValueData : passwordData,
        (__bridge id)kSecAttrAccessControl : (__bridge id)accessControl,
    };

    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
    CFRelease(accessControl);
    if (status != errSecSuccess) {
        qWarning() << "storeBiometricPassword: SecItemAdd failed:" << (int)status;
        if (status == errSecMissingEntitlement) {
            status = addGenericPasswordItem(kLegacyKeychainService, account, passwordData);
            if (status != errSecSuccess) {
                qWarning() << "storeBiometricPassword: legacy fallback failed:" << (int)status;
            }
        }
    }
    return status == errSecSuccess;
}

bool retrieveBiometricPassword(const QString& walletAddress, QString& outPassword) {
    NSString* account = walletAddress.toNSString();
    LAContext* context = [[LAContext alloc] init];
    if ([context respondsToSelector:@selector(setLocalizedReason:)]) {
        context.localizedReason = @"Unlock your Cinder wallet";
    }

    NSError* evalError = nil;
    if (![context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                              error:&evalError]) {
        return false;
    }

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block BOOL authOk = NO;
    [context evaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
            localizedReason:@"Unlock your Cinder wallet"
                      reply:^(BOOL success, NSError*) {
                          authOk = success;
                          dispatch_semaphore_signal(sema);
                      }];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    if (!authOk) {
        return false;
    }

    OSStatus status = copyKeychainPassword(kKeychainService, account, context, outPassword);
    if (status == errSecSuccess) {
        return true;
    }

    // One-time dev migration path: if the new biometric item is missing,
    // read the legacy item after the same biometric prompt, then rewrite it
    // under the stronger ACL-backed service and delete the legacy entry.
    if (status == errSecItemNotFound) {
        QString legacyPassword;
        const OSStatus legacyStatus =
            copyKeychainPassword(kLegacyKeychainService, account, context, legacyPassword);
        if (legacyStatus == errSecSuccess && !legacyPassword.isEmpty()) {
            if (storeBiometricPassword(walletAddress, legacyPassword)) {
                NSDictionary* legacyDeleteQuery = @{
                    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
                    (__bridge id)kSecAttrService : kLegacyKeychainService,
                    (__bridge id)kSecAttrAccount : account,
                };
                SecItemDelete((__bridge CFDictionaryRef)legacyDeleteQuery);
            }
            outPassword = legacyPassword;
            return true;
        }
        status = legacyStatus;
    }

    if (status != errSecUserCanceled && status != errSecAuthFailed) {
        qWarning() << "retrieveBiometricPassword: SecItemCopyMatching failed:" << (int)status;
    }
    return false;
}

bool hasStoredBiometricPassword(const QString& walletAddress) {
    NSString* account = walletAddress.toNSString();
    return keychainItemExists(kKeychainService, account) ||
           keychainItemExists(kLegacyKeychainService, account);
}

bool deleteBiometricPassword(const QString& walletAddress) {
    NSString* account = walletAddress.toNSString();

    NSDictionary* query = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : kKeychainService,
        (__bridge id)kSecAttrAccount : account,
    };

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
    NSDictionary* legacyQuery = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : kLegacyKeychainService,
        (__bridge id)kSecAttrAccount : account,
    };
    OSStatus legacyStatus = SecItemDelete((__bridge CFDictionaryRef)legacyQuery);
    const bool deletedCurrent = (status == errSecSuccess || status == errSecItemNotFound);
    const bool deletedLegacy =
        (legacyStatus == errSecSuccess || legacyStatus == errSecItemNotFound);
    return deletedCurrent && deletedLegacy;
}

// ── Sleep/Wake Power Notifications ───────────────────────────────

static std::function<void()> sleepCallback;
static std::function<void()> wakeCallback;
static id sleepObserver = nil;
static id wakeObserver = nil;

void registerSleepWakeCallbacks(std::function<void()> onSleep,
                                std::function<void()> onWake) {
    unregisterSleepWakeCallbacks();

    sleepCallback = std::move(onSleep);
    wakeCallback = std::move(onWake);

    NSNotificationCenter* center = [[NSWorkspace sharedWorkspace] notificationCenter];

    sleepObserver = [center addObserverForName:NSWorkspaceWillSleepNotification
                                       object:nil
                                        queue:[NSOperationQueue mainQueue]
                                   usingBlock:^(NSNotification*) {
                                       if (sleepCallback) {
                                           sleepCallback();
                                       }
                                   }];

    wakeObserver = [center addObserverForName:NSWorkspaceDidWakeNotification
                                      object:nil
                                       queue:[NSOperationQueue mainQueue]
                                  usingBlock:^(NSNotification*) {
                                      if (wakeCallback) {
                                          wakeCallback();
                                      }
                                  }];
}

void unregisterSleepWakeCallbacks() {
    NSNotificationCenter* center = [[NSWorkspace sharedWorkspace] notificationCenter];
    if (sleepObserver) {
        [center removeObserver:sleepObserver];
        sleepObserver = nil;
    }
    if (wakeObserver) {
        [center removeObserver:wakeObserver];
        wakeObserver = nil;
    }
    sleepCallback = nullptr;
    wakeCallback = nullptr;
}

// ── Claude Code OAuth Token ──────────────────────────────────────

bool readClaudeCodeOAuthToken(QString& outToken) {
    NSString* service = @"Claude Code-credentials";
    NSString* account = NSUserName();

    NSDictionary* query = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : service,
        (__bridge id)kSecAttrAccount : account,
        (__bridge id)kSecReturnData : @YES,
    };

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    if (status == errSecSuccess && result) {
        NSData* data = (__bridge NSData*)result;
        NSString* tokenStr = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        CFRelease(result);
        if (tokenStr) {
            outToken = QString::fromNSString(tokenStr);
            return true;
        }
    }
    if (result)
        CFRelease(result);

    if (status != errSecItemNotFound && status != errSecAuthFailed &&
        status != errSecUserCanceled) {
        qWarning() << "[MacUtils] Claude OAuth token read failed with OSStatus:" << status;
    }
    return false;
}
