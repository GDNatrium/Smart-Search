#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include <Geode/modify/LevelSearchLayer.hpp>
#include <Geode/modify/ProfilePage.hpp>

// I assume parseListLevels does something similar, but TodoReturn
std::vector<int> parseList(const std::string& s) {
    std::vector<int> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (numFromString<int>(item).isErr()) continue;

        result.push_back(numFromString<int>(item).unwrap());
        
    }
    return result;
}

std::unordered_map<std::string, std::string> parseKeyValue(const std::string& input) {
    std::unordered_map<std::string, std::string> result;
    size_t i = 0;

    while (i < input.size()) {
        size_t key_end = input.find(':', i);
        if (key_end == std::string::npos) break;
        std::string key = input.substr(i, key_end - i);
        i = key_end + 1;

        size_t value_end = input.find(':', i);
        std::string value;
        if (value_end == std::string::npos) {
            value = input.substr(i);
            i = input.size();
        }
        else {
            value = input.substr(i, value_end - i);
            i = value_end + 1;
        }

        result[key] = value;
    }

    return result;
}


GJLevelList* createList(std::string resString) {
    auto pos = resString.find('#');
    std::string firstSegment = (pos == std::string::npos) ? resString : resString.substr(0, pos);

    auto kv = parseKeyValue(firstSegment);
    auto list = GJLevelList::create();

    if (!kv["1"].empty() && numFromString<int>(kv["1"]).isOk())  list->m_listID = numFromString<int>(kv["1"]).unwrap();
    if (!kv["2"].empty())  list->m_listName = kv["2"];
    if (!kv["3"].empty())  list->m_listDesc = kv["3"];
    if (!kv["5"].empty() && numFromString<int>(kv["5"]).isOk())  list->m_listVersion = numFromString<int>(kv["5"]).unwrap();
    if (!kv["7"].empty() && numFromString<int>(kv["7"]).isOk())  list->m_difficulty = numFromString<int>(kv["7"]).unwrap();
    if (!kv["10"].empty() && numFromString<int>(kv["10"]).isOk()) list->m_downloads = numFromString<int>(kv["10"]).unwrap();
    if (!kv["14"].empty() && numFromString<int>(kv["14"]).isOk()) list->m_likes = numFromString<int>(kv["14"]).unwrap();
    if (!kv["19"].empty()) list->m_featured = numFromString<int>(kv["19"]).unwrapOr(0);
    if (!kv["28"].empty() && numFromString<int>(kv["28"]).isOk()) list->m_uploadDate = numFromString<int>(kv["28"]).unwrap();
    if (!kv["29"].empty() && numFromString<int>(kv["29"]).isOk()) list->m_updateDate = numFromString<int>(kv["29"]).unwrap();
    if (!kv["49"].empty() && numFromString<int>(kv["49"]).isOk()) list->m_accountID = numFromString<int>(kv["49"]).unwrap();
    if (!kv["50"].empty()) list->m_creatorName = kv["50"];
    if (!kv["51"].empty()) list->m_levels = parseList(kv["51"]);
    if (!kv["55"].empty() && numFromString<int>(kv["55"]).isOk()) list->m_diamonds = numFromString<int>(kv["55"]).unwrap();
    if (!kv["56"].empty() && numFromString<int>(kv["56"]).isOk()) list->m_levelsToClaim = numFromString<int>(kv["56"]).unwrap();

    return list;
}


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
            if (m_type == 0) {
                // Level Search
                auto glm = GameLevelManager::get();
                glm->m_levelDownloadDelegate = DownloadDelegate::get();
                if (numFromString<int>(search).isOk()) {
                    glm->downloadLevel(numFromString<int>(search).unwrap(), true);
                }
                else {
                    Notification::create("Something went wrong.", NotificationIcon::Warning)->show();
                }
            }

            if (m_type == 1) {
                // List Search
                auto req = web::WebRequest()
                    .userAgent("")
                    .bodyString(fmt::format("secret=Wmfd2893gb7&str={}&type=0", search));

                auto URL = "http://www.boomlings.com/database/getGJLevelLists.php";

                req.post(URL).listen([this, spinner, sender](auto* res) {
                    if (res->ok()) {
                        std::string resString = res->string().unwrapOr("Error");

                        log::debug("resString: {}", resString);

                        if (resString == "Error" || resString == "-1") {
                            Notification::create("List does not exist.", NotificationIcon::Warning)->show();
                            spinner->removeFromParentAndCleanup(true);
                            return;
                        }

                        spinner->removeFromParentAndCleanup(true);

                        auto list = createList(resString);
                        auto levelListLayer = LevelListLayer::create(list);

                        auto scene = CCScene::create();
                        scene->addChild(levelListLayer);
                        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.5f, scene));
                    }
                    else {
                        Notification::create("List Fetching Failed.", NotificationIcon::Error)->show();

                        LevelSearchLayer::onSearch(sender);
                    }
                    });
            }
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

                log::debug("resString: {}", resString);

                if (resString == "Error" || resString == "-1") {
                    Notification::create("User does not exist.", NotificationIcon::Warning)->show();
                    spinner->removeFromParentAndCleanup(true);
                    return;
                }

                std::istringstream stream(resString);
                std::string key, value, accID;
                while (std::getline(stream, key, ':') && std::getline(stream, value, ':')) {
                    if (key == "16") {
                        accID = value;
                        break;
                    }
                }

                if (numFromString<int>(accID).isOk()) {
                    auto userPage = ProfilePage::create(numFromString<int>(accID).unwrap(), false);
                    userPage->show();
                }
                else {
                    Notification::create("Something went wrong.", NotificationIcon::Warning)->show();
                }

                spinner->removeFromParentAndCleanup(true);
            }
            else {
                Notification::create("User Fetching Failed.", NotificationIcon::Error)->show();

                LevelSearchLayer::onSearchUser(nullptr);
            }
            });
	}
};

// Fix for Android bug, where the "My Levels" page would not load levels correctly until the "My Lists" page was loaded
#if defined(GEODE_IS_ANDROID)

class $modify(ProfilePage) {
    void onMyLevels(CCObject * sender) {
        auto lbl = LevelBrowserLayer::create(GJSearchObject::create(SearchType::UsersLevels, std::to_string(m_score->m_userID)));

        auto scene = CCScene::create();
        scene->addChild(lbl);

        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(.5f, scene));
    }
};

#endif