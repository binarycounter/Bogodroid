// com.unity.mobile.notifications: its Java plugin (UnityNotificationManager) plus the android.app.Notification
// classes it uses. AndroidNotificationCenter.Initialize() looks all of these up and throws if the class is missing.
// Nothing is ever shown: scheduling hands back the id, channels are remembered so the game can list them.
#ifndef __NOTIFICATIONS_H__
#define __NOTIFICATIONS_H__

#include "android.h"
#include "javac.h"
#include <map>
#include <set>

void InitJNINotificationClasses(FakeJni::Jvm* vm);

namespace jnivm {
namespace java { namespace lang {
    // Only used as a return type (Integer, may be null)
    class IntegerRef : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("java/lang/Integer")
    };
} }

namespace android { namespace app {
    class Notification : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("android/app/Notification")
        inline static FakeJni::JString EXTRA_TITLE = (FakeJni::JString) "android.title";
        inline static FakeJni::JString EXTRA_TEXT = (FakeJni::JString) "android.text";
        inline static FakeJni::JString EXTRA_SHOW_CHRONOMETER = (FakeJni::JString) "android.showChronometer";
        inline static FakeJni::JString EXTRA_BIG_TEXT = (FakeJni::JString) "android.bigText";
        inline static FakeJni::JString EXTRA_SHOW_WHEN = (FakeJni::JString) "android.showWhen";
        inline static int FLAG_AUTO_CANCEL = 16;
        inline static int FLAG_GROUP_SUMMARY = 512;
        std::shared_ptr<jnivm::android::os::Bundle> extras = std::make_shared<jnivm::android::os::Bundle>();
        int flags = 0;
        int number = 0;
        long when = 0;
        std::string group, sortKey;
        std::shared_ptr<FakeJni::JString> getGroup();
        std::shared_ptr<FakeJni::JString> getSortKey();
    };

    class NotificationStyle : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("android/app/Notification$Style")
    };

    class NotificationBigTextStyle : public NotificationStyle {
    public:
        DEFINE_CLASS_NAME("android/app/Notification$BigTextStyle", NotificationStyle)
        std::string text;
        NotificationBigTextStyle() {}
        std::shared_ptr<NotificationBigTextStyle> bigText(std::shared_ptr<FakeJni::JString> t);
    };

    class NotificationBuilder : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("android/app/Notification$Builder")
        std::shared_ptr<Notification> n = std::make_shared<Notification>();
        std::string channelId;
        std::shared_ptr<jnivm::android::os::Bundle> getExtras();
        std::shared_ptr<NotificationBuilder> setContentTitle(std::shared_ptr<jnivm::CharSequence> t);
        std::shared_ptr<NotificationBuilder> setContentText(std::shared_ptr<jnivm::CharSequence> t);
        std::shared_ptr<NotificationBuilder> setAutoCancel(bool v);
        std::shared_ptr<NotificationBuilder> setNumber(int v);
        std::shared_ptr<NotificationBuilder> setStyle(std::shared_ptr<NotificationStyle> s);
        std::shared_ptr<NotificationBuilder> setWhen(long v);
        std::shared_ptr<NotificationBuilder> setGroup(std::shared_ptr<FakeJni::JString> g);
        std::shared_ptr<NotificationBuilder> setGroupSummary(bool v);
        std::shared_ptr<NotificationBuilder> setSortKey(std::shared_ptr<FakeJni::JString> k);
        std::shared_ptr<NotificationBuilder> setShowWhen(bool v);
        std::shared_ptr<Notification> build();
    private:
        std::shared_ptr<NotificationBuilder> self();
    };
} }

namespace com { namespace unity { namespace androidnotifications {
    class NotificationCallback : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("com/unity/androidnotifications/NotificationCallback")
    };

    class UnityNotificationManager : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("com/unity/androidnotifications/UnityNotificationManager")
        inline static FakeJni::JString KEY_FIRE_TIME = (FakeJni::JString) "fireTime";
        inline static FakeJni::JString KEY_ID = (FakeJni::JString) "id";
        inline static FakeJni::JString KEY_INTENT_DATA = (FakeJni::JString) "data";
        inline static FakeJni::JString KEY_LARGE_ICON = (FakeJni::JString) "largeIcon";
        inline static FakeJni::JString KEY_REPEAT_INTERVAL = (FakeJni::JString) "repeatInterval";
        inline static FakeJni::JString KEY_NOTIFICATION = (FakeJni::JString) "unityNotification";
        inline static FakeJni::JString KEY_NOTIFICATION_ID = (FakeJni::JString) "com.unity.NotificationID";
        inline static FakeJni::JString KEY_SMALL_ICON = (FakeJni::JString) "smallIcon";
        inline static FakeJni::JString KEY_CHANNEL_ID = (FakeJni::JString) "channelID";
        inline static FakeJni::JString KEY_SHOW_IN_FOREGROUND = (FakeJni::JString) "com.unity.showInForeground";
        inline static FakeJni::JString KEY_BIG_LARGE_ICON = (FakeJni::JString) "com.unity.BigLargeIcon";
        inline static FakeJni::JString KEY_BIG_PICTURE = (FakeJni::JString) "com.unity.BigPicture";
        inline static FakeJni::JString KEY_BIG_CONTENT_TITLE = (FakeJni::JString) "com.unity.BigContentTytle"; // sic, as in the plugin
        inline static FakeJni::JString KEY_BIG_SUMMARY_TEXT = (FakeJni::JString) "com.unity.BigSummaryText";
        inline static FakeJni::JString KEY_BIG_CONTENT_DESCRIPTION = (FakeJni::JString) "com.unity.BigContentDescription";
        inline static FakeJni::JString KEY_BIG_SHOW_WHEN_COLLAPSED = (FakeJni::JString) "com.unity.BigShowWhenCollapsed";

        std::set<std::string> channels;
        std::set<int> pending;

        // callback is a JNI proxy for the C# NotificationCallback, not one of our objects: take any object
        static std::shared_ptr<UnityNotificationManager> getNotificationManagerImpl(std::shared_ptr<jnivm::android::app::Activity> activity, std::shared_ptr<FakeJni::JObject> callback);
        int getTargetSdk();
        int areNotificationsEnabled();
        void registerNotificationChannelGroup(std::shared_ptr<FakeJni::JString> id, std::shared_ptr<FakeJni::JString> name, std::shared_ptr<FakeJni::JString> description);
        void deleteNotificationChannelGroup(std::shared_ptr<FakeJni::JString> id);
        void registerNotificationChannel(std::shared_ptr<FakeJni::JString> id, std::shared_ptr<FakeJni::JString> name, int importance,
            std::shared_ptr<FakeJni::JString> description, bool enableLights, bool enableVibration, bool canBypassDnd, bool canShowBadge,
            std::shared_ptr<jnivm::Array<jlong>> vibrationPattern, int lockscreenVisibility, std::shared_ptr<FakeJni::JString> group);
        void deleteNotificationChannel(std::shared_ptr<FakeJni::JString> id);
        std::shared_ptr<jnivm::Array<FakeJni::JObject>> getNotificationChannels();
        int scheduleNotification(std::shared_ptr<jnivm::android::app::NotificationBuilder> builder, bool customized);
        bool canScheduleExactAlarms();
        int checkNotificationStatus(int id);
        bool checkIfPendingNotificationIsRegistered(int id);
        void cancelAllPendingNotificationIntents();
        void cancelPendingNotification(int id);
        void cancelDisplayedNotification(int id);
        void cancelAllNotifications();
        std::shared_ptr<jnivm::android::app::NotificationBuilder> createNotificationBuilder(std::shared_ptr<FakeJni::JString> channelId);
        void setupBigPictureStyle(std::shared_ptr<jnivm::android::app::NotificationBuilder> builder, std::shared_ptr<FakeJni::JString> largeIcon,
            std::shared_ptr<FakeJni::JString> picture, std::shared_ptr<FakeJni::JString> contentTitle, std::shared_ptr<FakeJni::JString> contentDescription,
            std::shared_ptr<FakeJni::JString> summaryText, bool showWhenCollapsed);
        std::shared_ptr<jnivm::android::app::Notification> getNotificationFromIntent(std::shared_ptr<jnivm::android::content::Intent> intent);
        void showNotificationSettings(std::shared_ptr<FakeJni::JString> channelId);

        static void setNotificationIcon(std::shared_ptr<jnivm::android::app::NotificationBuilder> builder, std::shared_ptr<FakeJni::JString> keyName, std::shared_ptr<FakeJni::JString> icon);
        static void setNotificationColor(std::shared_ptr<jnivm::android::app::NotificationBuilder> builder, int color);
        static std::shared_ptr<jnivm::java::lang::IntegerRef> getNotificationColor(std::shared_ptr<jnivm::android::app::Notification> notification);
        static void setNotificationUsesChronometer(std::shared_ptr<jnivm::android::app::NotificationBuilder> builder, bool usesChrono);
        static void setNotificationGroupAlertBehavior(std::shared_ptr<jnivm::android::app::NotificationBuilder> builder, int behavior);
        static int getNotificationGroupAlertBehavior(std::shared_ptr<jnivm::android::app::Notification> notification);
        static std::shared_ptr<FakeJni::JString> getNotificationChannelId(std::shared_ptr<jnivm::android::app::Notification> notification);
    };
} } }
}

#endif
