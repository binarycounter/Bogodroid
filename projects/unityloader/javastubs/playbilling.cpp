#include "playbilling.h"
#include "jnibridge.h"
#include "logging.h"

using namespace jnivm::com::android::billingclient::api;

static const int BILLING_UNAVAILABLE = 3;   // BillingClient.BillingResponseCode
static const int DISCONNECTED = 0;          // BillingClient.ConnectionState

std::shared_ptr<BillingResult> BillingResult::unavailable()
{
    return std::make_shared<BillingResult>(BILLING_UNAVAILABLE, "Google Play billing is not available on this device");
}

std::shared_ptr<FakeJni::JObject> PendingPurchasesParams::newBuilder() { return std::make_shared<PendingPurchasesParamsBuilder>(); }

std::shared_ptr<PendingPurchasesParamsBuilder> PendingPurchasesParamsBuilder::enableOneTimeProducts()
{
    return std::dynamic_pointer_cast<PendingPurchasesParamsBuilder>(shared_from_this());
}
std::shared_ptr<PendingPurchasesParamsBuilder> PendingPurchasesParamsBuilder::enablePrepaidPlans()
{
    return std::dynamic_pointer_cast<PendingPurchasesParamsBuilder>(shared_from_this());
}
std::shared_ptr<PendingPurchasesParams> PendingPurchasesParamsBuilder::build() { return std::make_shared<PendingPurchasesParams>(); }

std::shared_ptr<FakeJni::JObject> BillingClient::newBuilder(std::shared_ptr<FakeJni::JObject>) { return std::make_shared<BillingClientBuilder>(); }

void BillingClient::startConnection(std::shared_ptr<FakeJni::JObject> listener)
{
    printf("[BILLING] startConnection: no Google Play, answering BILLING_UNAVAILABLE\n");
    auto proxy = std::dynamic_pointer_cast<jnivm::bitter::jnibridge::JNIBridgeProxy>(listener);
    if (!proxy) {
        verbose("BILLING", "startConnection: listener isn't a JNI proxy, nothing to call back");
        return;
    }
    std::shared_ptr<jnivm::java::lang::Object> result = BillingResult::unavailable();
    jnivm::bitter::jnibridge::JNIBridge::invoke(proxy->nativeHandle, "com/android/billingclient/api/BillingClientStateListener",
        "onBillingSetupFinished", "(Lcom/android/billingclient/api/BillingResult;)V", result);
}

void BillingClient::endConnection() {}
bool BillingClient::isReady() { return false; }
int BillingClient::getConnectionState() { return DISCONNECTED; }
std::shared_ptr<BillingResult> BillingClient::isFeatureSupported(std::shared_ptr<FakeJni::JString>) { return BillingResult::unavailable(); }

std::shared_ptr<BillingResult> BillingClient::launchBillingFlow(std::shared_ptr<FakeJni::JObject>, std::shared_ptr<FakeJni::JObject>)
{
    printf("[BILLING] launchBillingFlow refused: billing unavailable\n");
    return BillingResult::unavailable();
}

void BillingClient::queryPurchasesAsync(std::shared_ptr<FakeJni::JObject>, std::shared_ptr<FakeJni::JObject>) { printf("[BILLING] queryPurchasesAsync ignored: not connected\n"); }
void BillingClient::queryProductDetailsAsync(std::shared_ptr<FakeJni::JObject>, std::shared_ptr<FakeJni::JObject>) { printf("[BILLING] queryProductDetailsAsync ignored: not connected\n"); }
void BillingClient::consumeAsync(std::shared_ptr<FakeJni::JObject>, std::shared_ptr<FakeJni::JObject>) { printf("[BILLING] consumeAsync ignored: not connected\n"); }
void BillingClient::acknowledgePurchase(std::shared_ptr<FakeJni::JObject>, std::shared_ptr<FakeJni::JObject>) { printf("[BILLING] acknowledgePurchase ignored: not connected\n"); }
void BillingClient::getBillingConfigAsync(std::shared_ptr<FakeJni::JObject>, std::shared_ptr<FakeJni::JObject>) { printf("[BILLING] getBillingConfigAsync ignored: not connected\n"); }

std::shared_ptr<BillingClientBuilder> BillingClientBuilder::setListener(std::shared_ptr<FakeJni::JObject>)
{
    return std::dynamic_pointer_cast<BillingClientBuilder>(shared_from_this());
}
std::shared_ptr<BillingClientBuilder> BillingClientBuilder::enablePendingPurchases(std::shared_ptr<PendingPurchasesParams>)
{
    return std::dynamic_pointer_cast<BillingClientBuilder>(shared_from_this());
}
std::shared_ptr<BillingClientBuilder> BillingClientBuilder::enablePendingPurchasesLegacy()
{
    return std::dynamic_pointer_cast<BillingClientBuilder>(shared_from_this());
}
std::shared_ptr<BillingClientBuilder> BillingClientBuilder::enableAutoServiceReconnection()
{
    return std::dynamic_pointer_cast<BillingClientBuilder>(shared_from_this());
}
std::shared_ptr<BillingClient> BillingClientBuilder::build() { return std::make_shared<BillingClient>(); }

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::android::billingclient::api::BillingResult) { FakeJni::Constructor<BillingResult> {} },
{ FakeJni::Function<&BillingResult::getResponseCode> {}, "getResponseCode", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingResult::getDebugMessage> {}, "getDebugMessage", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::android::billingclient::api::PendingPurchasesParams) { FakeJni::Constructor<PendingPurchasesParams> {} },
{ FakeJni::Function<&PendingPurchasesParams::newBuilder> {}, "newBuilder", FakeJni::JMethodID::STATIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::android::billingclient::api::PendingPurchasesParamsBuilder) { FakeJni::Constructor<PendingPurchasesParamsBuilder> {} },
{ FakeJni::Function<&PendingPurchasesParamsBuilder::enableOneTimeProducts> {}, "enableOneTimeProducts", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&PendingPurchasesParamsBuilder::enablePrepaidPlans> {}, "enablePrepaidPlans", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&PendingPurchasesParamsBuilder::build> {}, "build", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::android::billingclient::api::BillingClientStateListener) { FakeJni::Constructor<BillingClientStateListener> {} },
{ FakeJni::Function<&BillingClientStateListener::onBillingSetupFinished> {}, "onBillingSetupFinished", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClientStateListener::onBillingServiceDisconnected> {}, "onBillingServiceDisconnected", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::android::billingclient::api::PurchasesUpdatedListener) { FakeJni::Constructor<PurchasesUpdatedListener> {} },
{ FakeJni::Function<&PurchasesUpdatedListener::onPurchasesUpdated> {}, "onPurchasesUpdated", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::android::billingclient::api::BillingClient) { FakeJni::Constructor<BillingClient> {} },
{ FakeJni::Function<&BillingClient::newBuilder> {}, "newBuilder", FakeJni::JMethodID::STATIC },
{ FakeJni::Function<&BillingClient::startConnection> {}, "startConnection", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClient::endConnection> {}, "endConnection", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClient::isReady> {}, "isReady", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClient::getConnectionState> {}, "getConnectionState", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClient::isFeatureSupported> {}, "isFeatureSupported", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClient::launchBillingFlow> {}, "launchBillingFlow", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClient::queryPurchasesAsync> {}, "queryPurchasesAsync", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClient::queryProductDetailsAsync> {}, "queryProductDetailsAsync", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClient::consumeAsync> {}, "consumeAsync", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClient::acknowledgePurchase> {}, "acknowledgePurchase", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClient::getBillingConfigAsync> {}, "getBillingConfigAsync", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::android::billingclient::api::BillingClientBuilder) { FakeJni::Constructor<BillingClientBuilder> {} },
{ FakeJni::Function<&BillingClientBuilder::setListener> {}, "setListener", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClientBuilder::enablePendingPurchases> {}, "enablePendingPurchases", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClientBuilder::enablePendingPurchasesLegacy> {}, "enablePendingPurchases", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClientBuilder::enableAutoServiceReconnection> {}, "enableAutoServiceReconnection", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&BillingClientBuilder::build> {}, "build", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

void InitJNIPlayBillingClasses(FakeJni::Jvm* vm)
{
    vm->registerClass<BillingResult>();
    vm->registerClass<PendingPurchasesParams>();
    vm->registerClass<PendingPurchasesParamsBuilder>();
    vm->registerClass<BillingClientStateListener>();
    vm->registerClass<PurchasesUpdatedListener>();
    vm->registerClass<BillingClient>();
    vm->registerClass<BillingClientBuilder>();
}
