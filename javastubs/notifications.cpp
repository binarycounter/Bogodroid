#include "notifications.h"
#include "logging.h"

using namespace jnivm::android::app;
using namespace jnivm::com::unity::androidnotifications;

///// android.app.Notification and friends

std::shared_ptr<FakeJni::JString> Notification::getGroup() { return group.empty() ? nullptr : std::make_shared<FakeJni::JString>(group); }
std::shared_ptr<FakeJni::JString> Notification::getSortKey() { return sortKey.empty() ? nullptr : std::make_shared<FakeJni::JString>(sortKey); }

std::shared_ptr<NotificationBigTextStyle> NotificationBigTextStyle::bigText(std::shared_ptr<FakeJni::JString> t)
{
    text = t ? t->c_str() : "";
    return std::dynamic_pointer_cast<NotificationBigTextStyle>(shared_from_this());
}

std::shared_ptr<NotificationBuilder> NotificationBuilder::self() { return std::dynamic_pointer_cast<NotificationBuilder>(shared_from_this()); }

static std::string text(const std::shared_ptr<jnivm::CharSequence>& c)
{
    if (!c) return "";
    if (auto s = std::dynamic_pointer_cast<jnivm::String>(c)) return *s;
    return "";
}

std::shared_ptr<jnivm::android::os::Bundle> NotificationBuilder::getExtras() { return n->extras; }
std::shared_ptr<NotificationBuilder> NotificationBuilder::setContentTitle(std::shared_ptr<jnivm::CharSequence> t)
{
    n->extras->strings[Notification::EXTRA_TITLE] = text(t);
    return self();
}
std::shared_ptr<NotificationBuilder> NotificationBuilder::setContentText(std::shared_ptr<jnivm::CharSequence> t)
{
    n->extras->strings[Notification::EXTRA_TEXT] = text(t);
    return self();
}
std::shared_ptr<NotificationBuilder> NotificationBuilder::setAutoCancel(bool v)
{
    n->flags = v ? (n->flags | Notification::FLAG_AUTO_CANCEL) : (n->flags & ~Notification::FLAG_AUTO_CANCEL);
    return self();
}
std::shared_ptr<NotificationBuilder> NotificationBuilder::setNumber(int v) { n->number = v; return self(); }
std::shared_ptr<NotificationBuilder> NotificationBuilder::setStyle(std::shared_ptr<NotificationStyle>) { return self(); }
std::shared_ptr<NotificationBuilder> NotificationBuilder::setWhen(long v) { n->when = v; return self(); }
std::shared_ptr<NotificationBuilder> NotificationBuilder::setGroup(std::shared_ptr<FakeJni::JString> g) { n->group = g ? g->c_str() : ""; return self(); }
std::shared_ptr<NotificationBuilder> NotificationBuilder::setGroupSummary(bool v)
{
    n->flags = v ? (n->flags | Notification::FLAG_GROUP_SUMMARY) : (n->flags & ~Notification::FLAG_GROUP_SUMMARY);
    return self();
}
std::shared_ptr<NotificationBuilder> NotificationBuilder::setSortKey(std::shared_ptr<FakeJni::JString> k) { n->sortKey = k ? k->c_str() : ""; return self(); }
std::shared_ptr<NotificationBuilder> NotificationBuilder::setShowWhen(bool v) { n->extras->numbers[Notification::EXTRA_SHOW_WHEN] = v; return self(); }
std::shared_ptr<Notification> NotificationBuilder::build() { return n; }

///// UnityNotificationManager

std::shared_ptr<UnityNotificationManager> UnityNotificationManager::getNotificationManagerImpl(std::shared_ptr<jnivm::android::app::Activity>, std::shared_ptr<FakeJni::JObject>)
{
    static std::shared_ptr<UnityNotificationManager> instance = std::make_shared<UnityNotificationManager>();
    return instance;
}

int UnityNotificationManager::getTargetSdk() { return 26; }
int UnityNotificationManager::areNotificationsEnabled() { return 1; } // PermissionStatus.Allowed
void UnityNotificationManager::registerNotificationChannelGroup(std::shared_ptr<FakeJni::JString>, std::shared_ptr<FakeJni::JString>, std::shared_ptr<FakeJni::JString>) {}
void UnityNotificationManager::deleteNotificationChannelGroup(std::shared_ptr<FakeJni::JString>) {}

void UnityNotificationManager::registerNotificationChannel(std::shared_ptr<FakeJni::JString> id, std::shared_ptr<FakeJni::JString> name, int,
    std::shared_ptr<FakeJni::JString>, bool, bool, bool, bool, std::shared_ptr<jnivm::Array<jlong>>, int, std::shared_ptr<FakeJni::JString>)
{
    if (id) channels.insert(id->c_str());
    printf("[NOTIFY] channel registered: %s (%s)\n", id ? id->c_str() : "?", name ? name->c_str() : "");
}

void UnityNotificationManager::deleteNotificationChannel(std::shared_ptr<FakeJni::JString> id) { if (id) channels.erase(id->c_str()); }

std::shared_ptr<jnivm::Array<FakeJni::JObject>> UnityNotificationManager::getNotificationChannels()
{
    return std::make_shared<jnivm::Array<FakeJni::JObject>>(0); // channel wrappers aren't modelled
}

int UnityNotificationManager::scheduleNotification(std::shared_ptr<NotificationBuilder> builder, bool)
{
    int id = 0;
    if (builder) id = builder->n->extras->getInt(std::make_shared<FakeJni::JString>(KEY_ID), 0);
    if (id == 0) { static int next = 1000; id = next++; }
    pending.insert(id);
    printf("[NOTIFY] notification %d scheduled (not shown: no notification UI)\n", id);
    return id;
}

bool UnityNotificationManager::canScheduleExactAlarms() { return true; }
int UnityNotificationManager::checkNotificationStatus(int id) { return pending.count(id) ? 1 : 0; } // Scheduled / Unavailable
bool UnityNotificationManager::checkIfPendingNotificationIsRegistered(int id) { return pending.count(id) != 0; }
void UnityNotificationManager::cancelAllPendingNotificationIntents() { pending.clear(); }
void UnityNotificationManager::cancelPendingNotification(int id) { pending.erase(id); }
void UnityNotificationManager::cancelDisplayedNotification(int) {}
void UnityNotificationManager::cancelAllNotifications() { pending.clear(); }

std::shared_ptr<NotificationBuilder> UnityNotificationManager::createNotificationBuilder(std::shared_ptr<FakeJni::JString> channelId)
{
    auto b = std::make_shared<NotificationBuilder>();
    b->channelId = channelId ? channelId->c_str() : "";
    return b;
}

void UnityNotificationManager::setupBigPictureStyle(std::shared_ptr<NotificationBuilder> builder, std::shared_ptr<FakeJni::JString> largeIcon,
    std::shared_ptr<FakeJni::JString> picture, std::shared_ptr<FakeJni::JString> contentTitle, std::shared_ptr<FakeJni::JString> contentDescription,
    std::shared_ptr<FakeJni::JString> summaryText, bool showWhenCollapsed)
{
    if (!builder || !picture || std::string(picture->c_str()).empty()) return;
    auto e = builder->n->extras;
    e->putString(std::make_shared<FakeJni::JString>(KEY_BIG_LARGE_ICON), largeIcon);
    e->putString(std::make_shared<FakeJni::JString>(KEY_BIG_PICTURE), picture);
    e->putString(std::make_shared<FakeJni::JString>(KEY_BIG_CONTENT_TITLE), contentTitle);
    e->putString(std::make_shared<FakeJni::JString>(KEY_BIG_SUMMARY_TEXT), summaryText);
    e->putString(std::make_shared<FakeJni::JString>(KEY_BIG_CONTENT_DESCRIPTION), contentDescription);
    e->putBoolean(std::make_shared<FakeJni::JString>(KEY_BIG_SHOW_WHEN_COLLAPSED), showWhenCollapsed);
}

// The app was never opened from a notification.
std::shared_ptr<Notification> UnityNotificationManager::getNotificationFromIntent(std::shared_ptr<jnivm::android::content::Intent>) { return nullptr; }
void UnityNotificationManager::showNotificationSettings(std::shared_ptr<FakeJni::JString>) {}

void UnityNotificationManager::setNotificationIcon(std::shared_ptr<NotificationBuilder> builder, std::shared_ptr<FakeJni::JString> keyName, std::shared_ptr<FakeJni::JString> icon)
{
    if (builder) builder->n->extras->putString(keyName, icon);
}
void UnityNotificationManager::setNotificationColor(std::shared_ptr<NotificationBuilder>, int) {}
std::shared_ptr<jnivm::java::lang::IntegerRef> UnityNotificationManager::getNotificationColor(std::shared_ptr<Notification>) { return nullptr; }
void UnityNotificationManager::setNotificationUsesChronometer(std::shared_ptr<NotificationBuilder> builder, bool v)
{
    if (builder) builder->n->extras->numbers[Notification::EXTRA_SHOW_CHRONOMETER] = v;
}
void UnityNotificationManager::setNotificationGroupAlertBehavior(std::shared_ptr<NotificationBuilder>, int) {}
int UnityNotificationManager::getNotificationGroupAlertBehavior(std::shared_ptr<Notification>) { return 0; }
std::shared_ptr<FakeJni::JString> UnityNotificationManager::getNotificationChannelId(std::shared_ptr<Notification>) { return nullptr; }

///// Descriptors

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::app::Notification) { FakeJni::Constructor<Notification> {} },
{ FakeJni::Field<&Notification::EXTRA_TITLE> {}, "EXTRA_TITLE", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Notification::EXTRA_TEXT> {}, "EXTRA_TEXT", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Notification::EXTRA_SHOW_CHRONOMETER> {}, "EXTRA_SHOW_CHRONOMETER", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Notification::EXTRA_BIG_TEXT> {}, "EXTRA_BIG_TEXT", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Notification::EXTRA_SHOW_WHEN> {}, "EXTRA_SHOW_WHEN", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Notification::FLAG_AUTO_CANCEL> {}, "FLAG_AUTO_CANCEL", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Notification::FLAG_GROUP_SUMMARY> {}, "FLAG_GROUP_SUMMARY", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Notification::extras> {}, "extras", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Notification::flags> {}, "flags", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Notification::number> {}, "number", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Notification::when> {}, "when", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Function<&Notification::getGroup> {}, "getGroup", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Notification::getSortKey> {}, "getSortKey", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::app::NotificationStyle) { FakeJni::Constructor<NotificationStyle> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::app::NotificationBigTextStyle) { FakeJni::Constructor<NotificationBigTextStyle> {} },
{ FakeJni::Function<&NotificationBigTextStyle::bigText> {}, "bigText", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::app::NotificationBuilder) { FakeJni::Constructor<NotificationBuilder> {} },
{ FakeJni::Function<&NotificationBuilder::getExtras> {}, "getExtras", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::setContentTitle> {}, "setContentTitle", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::setContentText> {}, "setContentText", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::setAutoCancel> {}, "setAutoCancel", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::setNumber> {}, "setNumber", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::setStyle> {}, "setStyle", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::setWhen> {}, "setWhen", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::setGroup> {}, "setGroup", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::setGroupSummary> {}, "setGroupSummary", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::setSortKey> {}, "setSortKey", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::setShowWhen> {}, "setShowWhen", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&NotificationBuilder::build> {}, "build", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity::androidnotifications::NotificationCallback) { FakeJni::Constructor<NotificationCallback> {} },
END_NATIVE_DESCRIPTOR

#define KEYFIELD(k) { FakeJni::Field<&UnityNotificationManager::k> {}, #k, FakeJni::JFieldID::STATIC }
BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity::androidnotifications::UnityNotificationManager) { FakeJni::Constructor<UnityNotificationManager> {} },
KEYFIELD(KEY_FIRE_TIME), KEYFIELD(KEY_ID), KEYFIELD(KEY_INTENT_DATA), KEYFIELD(KEY_LARGE_ICON), KEYFIELD(KEY_REPEAT_INTERVAL),
KEYFIELD(KEY_NOTIFICATION), KEYFIELD(KEY_NOTIFICATION_ID), KEYFIELD(KEY_SMALL_ICON), KEYFIELD(KEY_CHANNEL_ID),
KEYFIELD(KEY_SHOW_IN_FOREGROUND), KEYFIELD(KEY_BIG_LARGE_ICON), KEYFIELD(KEY_BIG_PICTURE), KEYFIELD(KEY_BIG_CONTENT_TITLE),
KEYFIELD(KEY_BIG_SUMMARY_TEXT), KEYFIELD(KEY_BIG_CONTENT_DESCRIPTION), KEYFIELD(KEY_BIG_SHOW_WHEN_COLLAPSED),
{ FakeJni::Function<&UnityNotificationManager::getNotificationManagerImpl> {}, "getNotificationManagerImpl", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&UnityNotificationManager::getTargetSdk> {}, "getTargetSdk", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::areNotificationsEnabled> {}, "areNotificationsEnabled", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::registerNotificationChannelGroup> {}, "registerNotificationChannelGroup", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::deleteNotificationChannelGroup> {}, "deleteNotificationChannelGroup", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::registerNotificationChannel> {}, "registerNotificationChannel", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::deleteNotificationChannel> {}, "deleteNotificationChannel", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::getNotificationChannels> {}, "getNotificationChannels", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::scheduleNotification> {}, "scheduleNotification", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::canScheduleExactAlarms> {}, "canScheduleExactAlarms", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::checkNotificationStatus> {}, "checkNotificationStatus", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::checkIfPendingNotificationIsRegistered> {}, "checkIfPendingNotificationIsRegistered", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::cancelAllPendingNotificationIntents> {}, "cancelAllPendingNotificationIntents", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::cancelPendingNotification> {}, "cancelPendingNotification", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::cancelDisplayedNotification> {}, "cancelDisplayedNotification", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::cancelAllNotifications> {}, "cancelAllNotifications", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::createNotificationBuilder> {}, "createNotificationBuilder", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::setupBigPictureStyle> {}, "setupBigPictureStyle", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::getNotificationFromIntent> {}, "getNotificationFromIntent", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::showNotificationSettings> {}, "showNotificationSettings", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&UnityNotificationManager::setNotificationIcon> {}, "setNotificationIcon", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&UnityNotificationManager::setNotificationColor> {}, "setNotificationColor", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&UnityNotificationManager::getNotificationColor> {}, "getNotificationColor", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&UnityNotificationManager::setNotificationUsesChronometer> {}, "setNotificationUsesChronometer", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&UnityNotificationManager::setNotificationGroupAlertBehavior> {}, "setNotificationGroupAlertBehavior", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&UnityNotificationManager::getNotificationGroupAlertBehavior> {}, "getNotificationGroupAlertBehavior", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&UnityNotificationManager::getNotificationChannelId> {}, "getNotificationChannelId", FakeJni::JMethodID::STATIC },
END_NATIVE_DESCRIPTOR
#undef KEYFIELD

void InitJNINotificationClasses(FakeJni::Jvm* vm)
{
    vm->registerClass<Notification>();
    vm->registerClass<NotificationStyle>();
    vm->registerClass<NotificationBigTextStyle>();
    vm->registerClass<NotificationBuilder>();
    vm->registerClass<NotificationCallback>();
    vm->registerClass<UnityNotificationManager>();
}
