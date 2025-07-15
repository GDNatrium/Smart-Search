#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include <Geode/modify/LevelSearchLayer.hpp>

class DownloadDelegate : public LevelDownloadDelegate {
public:
    static DownloadDelegate* get() {
        static DownloadDelegate instance;
        return &instance;
    }

    void levelDownloadFinished(GJGameLevel* level) override {
        GameLevelManager::get()->m_levelDownloadDelegate = nullptr;

        auto levelSearchLayer = CCDirector::sharedDirector()->getRunningScene()->getChildByType<LevelSearchLayer>(0);
        if (auto spinner = levelSearchLayer->getChildByID("loading-spinner")) spinner->removeFromParentAndCleanup(true);

        level->m_gauntletLevel = false;
        level->m_gauntletLevel2 = false;

        auto scene = CCScene::create();
        scene->addChild(LevelInfoLayer::create(level, false));

        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(.5f, scene));
    }

    void levelDownloadFailed(int) override {
        GameLevelManager::get()->m_levelDownloadDelegate = nullptr;

        Notification::create("Level Download Failed.", NotificationIcon::Error)->show();

        auto levelSearchLayer = CCDirector::sharedDirector()->getRunningScene()->getChildByType<LevelSearchLayer>(0);
        auto lvlBrowser = LevelBrowserLayer::create(GJSearchObject::create(SearchType::Search, levelSearchLayer->m_searchInput->getString()));

        if (auto spinner = levelSearchLayer->getChildByID("loading-spinner")) spinner->removeFromParentAndCleanup(true);

        switchToScene(lvlBrowser);
    }
};

class $modify(LevelSearchLayer) {
	void onSearch(CCObject * sender) {
        auto spinner = LoadingSpinner::create(80);
        spinner->setPosition(CCDirector::sharedDirector()->getWinSize() / 2);
        this->addChild(spinner);

        std::string search = m_searchInput->getString();

        auto end = search.find_last_not_of(' ');
        bool isID = end != std::string::npos && std::all_of(search.begin(), search.begin() + end + 1, ::isdigit);

        if (isID) {
            auto glm = GameLevelManager::get();
            glm->m_levelDownloadDelegate = DownloadDelegate::get();
            glm->downloadLevel(std::stoi(search), true);
        }
        else {
            spinner->removeFromParentAndCleanup(true);
            LevelSearchLayer::onSearch(sender);
        }
	}

	void onSearchUser(CCObject* sender) {
        auto spinner = LoadingSpinner::create(80);
        spinner->setPosition(CCDirector::sharedDirector()->getWinSize() / 2);
        this->addChild(spinner);

        std::string search = m_searchInput->getString();

        auto req = web::WebRequest()
            .userAgent("")
            .bodyString(fmt::format("secret=Wmfd2893gb7&str={}", search));

        auto URL = "http://www.boomlings.com/database/getGJUsers20.php";

        req.post(URL).listen([this, spinner](auto* res) {
            if (res->ok()) {
                std::string resString = res->string().unwrapOr("Error");

                std::istringstream stream(resString);
                std::string key, value, accID;
                while (std::getline(stream, key, ':') && std::getline(stream, value, ':')) {
                    if (key == "16") {
                        accID = value;
                        break;
                    }
                }

                auto userPage = ProfilePage::create(stoi(accID), false);
                userPage->show();

                spinner->removeFromParentAndCleanup(true);
            }
            else {
                Notification::create("User Fetching Failed.", NotificationIcon::Error)->show();

                LevelSearchLayer::onSearchUser(nullptr);
            }
            });
	}
};
