// Google Play Billing (com.android.billingclient), as Unity IAP's Google Play store uses it. There is no Google Play
// on a handheld, so the client never connects: startConnection() answers BILLING_UNAVAILABLE, the same as a phone
// without the Play Store, and IAP reports a normal "purchasing unavailable" initialisation failure that games handle.
#ifndef __PLAYBILLING_H__
#define __PLAYBILLING_H__

#include "baron/baron.h"
#include "javac.h"
#include <string>

void InitJNIPlayBillingClasses(FakeJni::Jvm* vm);

namespace jnivm { namespace com { namespace android { namespace billingclient { namespace api {
    class BillingResult : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("com/android/billingclient/api/BillingResult")
        int code = 3;
        std::string message;
        BillingResult() {}
        BillingResult(int c, const std::string& m) : code(c), message(m) {}
        int getResponseCode() { return code; }
        std::shared_ptr<FakeJni::JString> getDebugMessage() { return std::make_shared<FakeJni::JString>(message); }
        static std::shared_ptr<BillingResult> unavailable();
    };

    class PendingPurchasesParams : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("com/android/billingclient/api/PendingPurchasesParams")
        static std::shared_ptr<FakeJni::JObject> newBuilder();
    };

    class PendingPurchasesParamsBuilder : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("com/android/billingclient/api/PendingPurchasesParams$Builder")
        std::shared_ptr<PendingPurchasesParamsBuilder> enableOneTimeProducts();
        std::shared_ptr<PendingPurchasesParamsBuilder> enablePrepaidPlans();
        std::shared_ptr<PendingPurchasesParams> build();
    };

    // The listener interface, so the loader can call the C# proxy's onBillingSetupFinished through JNIBridge.
    class BillingClientStateListener : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("com/android/billingclient/api/BillingClientStateListener")
        void onBillingSetupFinished(std::shared_ptr<BillingResult> result) {}
        void onBillingServiceDisconnected() {}
    };

    // Passed to BillingClient.Builder.setListener(); purchases never complete, so it's never called.
    class PurchasesUpdatedListener : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("com/android/billingclient/api/PurchasesUpdatedListener")
        void onPurchasesUpdated(std::shared_ptr<BillingResult> result, std::shared_ptr<jnivm::java::util::List> purchases) {}
    };

    class BillingClient : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("com/android/billingclient/api/BillingClient")
        static std::shared_ptr<FakeJni::JObject> newBuilder(std::shared_ptr<FakeJni::JObject> context);
        void startConnection(std::shared_ptr<FakeJni::JObject> listener);
        void endConnection();
        bool isReady();
        int getConnectionState();
        std::shared_ptr<BillingResult> isFeatureSupported(std::shared_ptr<FakeJni::JString> feature);
        std::shared_ptr<BillingResult> launchBillingFlow(std::shared_ptr<FakeJni::JObject> activity, std::shared_ptr<FakeJni::JObject> params);
        // Never called after a failed connection; just logged in case a game tries anyway.
        void queryPurchasesAsync(std::shared_ptr<FakeJni::JObject> params, std::shared_ptr<FakeJni::JObject> listener);
        void queryProductDetailsAsync(std::shared_ptr<FakeJni::JObject> params, std::shared_ptr<FakeJni::JObject> listener);
        void consumeAsync(std::shared_ptr<FakeJni::JObject> params, std::shared_ptr<FakeJni::JObject> listener);
        void acknowledgePurchase(std::shared_ptr<FakeJni::JObject> params, std::shared_ptr<FakeJni::JObject> listener);
        void getBillingConfigAsync(std::shared_ptr<FakeJni::JObject> params, std::shared_ptr<FakeJni::JObject> listener);
    };

    class BillingClientBuilder : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("com/android/billingclient/api/BillingClient$Builder")
        std::shared_ptr<BillingClientBuilder> setListener(std::shared_ptr<FakeJni::JObject> listener);
        std::shared_ptr<BillingClientBuilder> enablePendingPurchases(std::shared_ptr<PendingPurchasesParams> params);
        std::shared_ptr<BillingClientBuilder> enablePendingPurchasesLegacy();
        std::shared_ptr<BillingClientBuilder> enableAutoServiceReconnection();
        std::shared_ptr<BillingClient> build();
    };
} } } } }

#endif
