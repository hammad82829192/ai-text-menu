// AI Text Menu - by hammadus
#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

static std::vector<std::pair<std::string, std::string>> g_history;

class AIPopup : public Popup<> {
protected:
    TextInput* m_input = nullptr;
    ScrollLayer* m_scroll = nullptr;
    EventListener<web::WebTask> m_listener;
    bool m_busy = false;

    bool setup() override {
        this->setTitle("AI Text Menu");
        auto size = m_mainLayer->getContentSize();

        auto bg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
        bg->setColor({0, 0, 0});
        bg->setOpacity(90);
        bg->setContentSize({330.f, 120.f});
        bg->setPosition({size.width / 2, 150.f});
        m_mainLayer->addChild(bg, -1);

        m_scroll = ScrollLayer::create({320.f, 110.f});
        m_scroll->setPosition({size.width / 2 - 160.f, 95.f});
        m_mainLayer->addChild(m_scroll);

        m_input = TextInput::create(250.f, "Ask the AI...", "chatFont.fnt");
        m_input->setMaxCharCount(400);
        m_input->setPosition({size.width / 2 - 35.f, 60.f});
        m_mainLayer->addChild(m_input);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(menu);

        auto sendBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Send", 60, true, "goldFont.fnt", "GJ_button_01.png", 30.f, 0.7f),
            this, menu_selector(AIPopup::onSend));
        sendBtn->setPosition({size.width / 2 + 135.f, 60.f});
        menu->addChild(sendBtn);

        auto clearBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Clear Chat", 90, true, "goldFont.fnt", "GJ_button_06.png", 30.f, 0.7f),
            this, menu_selector(AIPopup::onClear));
        clearBtn->setPosition({size.width / 2, 22.f});
        menu->addChild(clearBtn);

        this->showText(g_history.empty() ? "Hi! Type a message below and press Send." : g_history.back().second);
        return true;
    }

    void showText(std::string const& text) {
        auto content = m_scroll->m_contentLayer;
        content->removeAllChildren();
        auto area = TextArea::create(text, "chatFont.fnt", 0.8f, 300.f, {0.5f, 0.5f}, 16.f, false);
        float h = std::max(area->getContentSize().height + 10.f, 110.f);
        content->setContentSize({320.f, h});
        area->setAnchorPoint({0.5f, 0.5f});
        area->setPosition({160.f, h - 5.f - area->getContentSize().height / 2});
        content->addChild(area);
        m_scroll->scrollToTop();
    }

    void onClear(CCObject*) {
        g_history.clear();
        this->showText("Chat cleared.");
    }

    void onSend(CCObject*) {
        if (m_busy) return;
        auto prompt = m_input->getString();
        if (prompt.empty()) return;

        auto key = Mod::get()->getSettingValue<std::string>("api-key");
        if (key.empty()) {
            this->showText("No API key set. Open Geode > AI Text Menu > Settings and paste your key.");
            return;
        }
        auto url = Mod::get()->getSettingValue<std::string>("api-url");
        auto model = Mod::get()->getSettingValue<std::string>("model");

        g_history.push_back({"user", prompt});
        if (g_history.size() > 12) g_history.erase(g_history.begin(), g_history.begin() + 2);

        auto messages = matjson::Value::array();
        for (auto& [role, text] : g_history) {
            auto m = matjson::Value::object();
            m["role"] = role;
            m["content"] = text;
            messages.push(m);
        }
        auto body = matjson::Value::object();
        body["model"] = model;
        body["messages"] = messages;

        auto req = web::WebRequest();
        req.header("Authorization", "Bearer " + key);
        req.header("Content-Type", "application/json");
        req.bodyJSON(body);

        m_busy = true;
        m_input->setString("");
        this->showText("Thinking...");

        m_listener.bind([this](web::WebTask::Event* e) {
            if (auto res = e->getValue()) {
                m_busy = false;
                auto json = res->json();
                if (json.isErr()) {
                    this->showText("Bad response from server.");
                    return;
                }
                auto v = json.unwrap();
                auto reply = v["choices"][0]["message"]["content"].asString();
                if (reply.isOk()) {
                    g_history.push_back({"assistant", reply.unwrap()});
                    this->showText(reply.unwrap());
                } else {
                    auto err = v["error"]["message"].asString();
                    this->showText("Error: " + (err.isOk() ? err.unwrap() : std::string("unknown")));
                }
            } else if (e->isCancelled()) {
                m_busy = false;
                this->showText("Request cancelled.");
            }
        });
        m_listener.setFilter(req.post(url));
    }

public:
    static AIPopup* create() {
        auto ret = new AIPopup();
        if (ret && ret->initAnchored(360.f, 240.f)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

class $modify(AIMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto label = CCLabelBMFont::create("AI", "bigFont.fnt");
        auto spr = CircleButtonSprite::create(label);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AIMenuLayer::onOpenAI));
        btn->setID("ai-text-menu-button"_spr);

        if (auto menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }
        return true;
    }

    void onOpenAI(CCObject*) {
        if (auto p = AIPopup::create()) p->show();
    }
};
