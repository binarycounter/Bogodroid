#include "playgames.h"
#include "logging.h"


std::shared_ptr<jnivm::com::google::android::gms::games::AchievementsClient> jnivm::com::google::android::gms::games::PlayGames::getAchievementsClient(std::shared_ptr<jnivm::android::app::Activity> activity) {
    return nullptr;
}

std::shared_ptr<jnivm::com::google::android::gms::games::EventsClient> jnivm::com::google::android::gms::games::PlayGames::getEventsClient(std::shared_ptr<jnivm::android::app::Activity> activity) {
    return nullptr;
}

std::shared_ptr<jnivm::com::google::android::gms::games::GamesSignInClient> jnivm::com::google::android::gms::games::PlayGames::getGamesSignInClient(std::shared_ptr<jnivm::android::app::Activity> activity) {
    return nullptr;
}

std::shared_ptr<jnivm::com::google::android::gms::games::LeaderboardsClient> jnivm::com::google::android::gms::games::PlayGames::getLeaderboardsClient(std::shared_ptr<jnivm::android::app::Activity> activity) {
    return nullptr;
}

std::shared_ptr<jnivm::com::google::android::gms::games::PlayerStatsClient> jnivm::com::google::android::gms::games::PlayGames::getPlayerStatsClient(std::shared_ptr<jnivm::android::app::Activity> activity) {
    return nullptr;
}

std::shared_ptr<jnivm::com::google::android::gms::games::PlayersClient> jnivm::com::google::android::gms::games::PlayGames::getPlayersClient(std::shared_ptr<jnivm::android::app::Activity> activity) {
    return nullptr;
}

std::shared_ptr<jnivm::com::google::android::gms::games::SnapshotsClient> jnivm::com::google::android::gms::games::PlayGames::getSnapshotsClient(std::shared_ptr<jnivm::android::app::Activity> activity) {
    return nullptr;
}

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::google::android::gms::games::AchievementsClient) { FakeJni::Constructor<AchievementsClient> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::google::android::gms::games::EventsClient) { FakeJni::Constructor<EventsClient> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::google::android::gms::games::GamesSignInClient) { FakeJni::Constructor<GamesSignInClient> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::google::android::gms::games::LeaderboardsClient) { FakeJni::Constructor<LeaderboardsClient> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::google::android::gms::games::PlayerStatsClient) { FakeJni::Constructor<PlayerStatsClient> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::google::android::gms::games::PlayersClient) { FakeJni::Constructor<PlayersClient> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::google::android::gms::games::SnapshotsClient) { FakeJni::Constructor<SnapshotsClient> {} },
END_NATIVE_DESCRIPTOR


BEGIN_NATIVE_DESCRIPTOR(jnivm::com::google::android::gms::games::PlayGames) {
    FakeJni::Constructor<PlayGames>{},
},
{ FakeJni::Function<&PlayGames::getAchievementsClient>{}, "getAchievementsClient", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&PlayGames::getEventsClient>{}, "getEventsClient", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&PlayGames::getGamesSignInClient>{}, "getGamesSignInClient", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&PlayGames::getLeaderboardsClient>{}, "getLeaderboardsClient", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&PlayGames::getPlayerStatsClient>{}, "getPlayerStatsClient", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&PlayGames::getPlayersClient>{}, "getPlayersClient", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&PlayGames::getSnapshotsClient>{}, "getSnapshotsClient", FakeJni::JMethodID::STATIC },
END_NATIVE_DESCRIPTOR


void InitJNIPlayGamesClasses(FakeJni::Jvm* vm)
{
    verbose("JBRIDGE", "Initializing Play Games JNI Classes");
    vm->registerClass<jnivm::com::google::android::gms::games::AchievementsClient>();
    vm->registerClass<jnivm::com::google::android::gms::games::EventsClient>();
    vm->registerClass<jnivm::com::google::android::gms::games::GamesSignInClient>();
    vm->registerClass<jnivm::com::google::android::gms::games::LeaderboardsClient>();
    vm->registerClass<jnivm::com::google::android::gms::games::PlayerStatsClient>();
    vm->registerClass<jnivm::com::google::android::gms::games::PlayersClient>();
    vm->registerClass<jnivm::com::google::android::gms::games::SnapshotsClient>();
    vm->registerClass<jnivm::com::google::android::gms::games::PlayGames>();
}