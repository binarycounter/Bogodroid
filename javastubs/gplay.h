#ifndef __PLAYGAMES_H__
#define __PLAYGAMES_H__

#include "android.h"
#include "baron/baron.h"
#include "javac.h"

void InitJNIGooglePlayClasses(FakeJni::Jvm* vm);

namespace jnivm {
namespace com {
    namespace google {
        namespace android {
            namespace play {
                namespace core {
                    namespace review {
                        class ReviewManager : public FakeJni::JObject {
                        public:
                            DEFINE_CLASS_NAME("com/google/android/play/core/review/ReviewManager")
                        };
                        class ReviewManagerFactory : public FakeJni::JObject {
                        public:
                            DEFINE_CLASS_NAME("com/google/android/play/core/review/ReviewManagerFactory")
                            static std::shared_ptr<ReviewManager> create(std::shared_ptr<jnivm::android::content::Context> context);
                        };
                    }
                }
            }

            namespace gms {
                namespace games {
                    class AchievementsClient : public FakeJni::JObject {
                    public:
                        DEFINE_CLASS_NAME("com/google/android/gms/games/AchievementsClient")
                    };

                    class EventsClient : public FakeJni::JObject {
                    public:
                        DEFINE_CLASS_NAME("com/google/android/gms/games/EventsClient")
                    };

                    class GamesSignInClient : public FakeJni::JObject {
                    public:
                        DEFINE_CLASS_NAME("com/google/android/gms/games/GamesSignInClient")
                    };

                    class LeaderboardsClient : public FakeJni::JObject {
                    public:
                        DEFINE_CLASS_NAME("com/google/android/gms/games/LeaderboardsClient")
                    };

                    class PlayerStatsClient : public FakeJni::JObject {
                    public:
                        DEFINE_CLASS_NAME("com/google/android/gms/games/PlayerStatsClient")
                    };

                    class PlayersClient : public FakeJni::JObject {
                    public:
                        DEFINE_CLASS_NAME("com/google/android/gms/games/PlayersClient")
                    };

                    class SnapshotsClient : public FakeJni::JObject {
                    public:
                        DEFINE_CLASS_NAME("com/google/android/gms/games/SnapshotsClient")
                    };

                    class PlayGames : public FakeJni::JObject {
                    public:
                        DEFINE_CLASS_NAME("com/google/android/gms/games/PlayGames")
                        static std::shared_ptr<jnivm::com::google::android::gms::games::AchievementsClient> getAchievementsClient(std::shared_ptr<jnivm::android::app::Activity> activity);
                        static std::shared_ptr<jnivm::com::google::android::gms::games::EventsClient> getEventsClient(std::shared_ptr<jnivm::android::app::Activity> activity);
                        static std::shared_ptr<jnivm::com::google::android::gms::games::GamesSignInClient> getGamesSignInClient(std::shared_ptr<jnivm::android::app::Activity> activity);
                        static std::shared_ptr<jnivm::com::google::android::gms::games::LeaderboardsClient> getLeaderboardsClient(std::shared_ptr<jnivm::android::app::Activity> activity);
                        static std::shared_ptr<jnivm::com::google::android::gms::games::PlayerStatsClient> getPlayerStatsClient(std::shared_ptr<jnivm::android::app::Activity> activity);
                        static std::shared_ptr<jnivm::com::google::android::gms::games::PlayersClient> getPlayersClient(std::shared_ptr<jnivm::android::app::Activity> activity);
                        static std::shared_ptr<jnivm::com::google::android::gms::games::SnapshotsClient> getSnapshotsClient(std::shared_ptr<jnivm::android::app::Activity> activity);
                    };

                    class PlayGamesSdk : public FakeJni::JObject {
                    public:
                        DEFINE_CLASS_NAME("com/google/android/gms/games/PlayGamesSdk")
                        static void initialize(std::shared_ptr<jnivm::android::content::Context> context);
                    };
                }
            }
        }
    }
}
}

#endif