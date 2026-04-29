#include "game/scenes/scene-game.h"

#include "core/context.h"
#include "game/controllers/gameplay-camera-controller.h"
#include "game/render/world-render-clip.h"
#include "game/shaders/gameplay-shader-controller.h"
#include "game/ui/ingame-hud-overlay.h"

#include <algorithm>
#include <string>
#include <vector>

GameScene::GameScene(void)
    : shipAutoFollowEnabled(true),
      playerShipFolderPath{},
      playerExperiencePointsCurrent(0)
      //playerVfxFolderPath("assets/images/vfx/vfx-speedwhitedeux"),
      //shipVfx{}
{
}

void GameScene::setExperiencePointsCurrent(int points)
{
    this->playerExperiencePointsCurrent = (std::max)(0, points);
}

void GameScene::populateMoneyDemoData(void)
{
    GetIngameHudOverlay().getMoneyWidget().setCurrencyEntries(
        std::vector<MoneyWidget::CurrencyEntry>{
            {MoneyWidget::CurrencyType::GOLD, "assets/images/ui-scene-game/money-gold.png", 1250000},
            {MoneyWidget::CurrencyType::RUBIES, "assets/images/ui-scene-game/money-rubies.png", 3500}
        });
}

void GameScene::populateGuildMortarData(void)
{
    GuildMortarWidget& guildMortarWidget = GetIngameHudOverlay().getGuildMortarWidget();
    guildMortarWidget.setMortarLevels(
        std::vector<GuildMortarWidget::MortarLevelEntry>{
            {"Mortier I", 2500, 5.0f, 12, 50000, 0, 180, 35},
            {"Mortier II", 4200, 4.5f, 14, 85000, 0, 260, 55},
            {"Mortier III", 6100, 4.0f, 16, 120000, 0, 340, 80},
            {"Mortier IV", 7500, 4.0f, 18, 150000, 0, 480, 120}
        });
    guildMortarWidget.setSelectedMortarLevelIndex(2);
    guildMortarWidget.setMortarEnabled(false);
    guildMortarWidget.setTreasuryGoldAmount(3147765460LL);
    guildMortarWidget.setTreasuryRubiesAmount(396083);
    guildMortarWidget.setTreasuryPearlsAmount(820450);
    guildMortarWidget.setTreasuryCrystalsAmount(40320);
}

void GameScene::populateGuildTowerData(void)
{
    GuildTowerWidget& guildTowerWidget = GetIngameHudOverlay().getGuildTowerWidget();
    guildTowerWidget.setTowerLevels(
        std::vector<GuildTowerWidget::TowerLevelEntry>{
            {"Aedan", 1, 1800, 2.8f, 11, 65000, 65000, 30, 1200, 15000, 85000, 120, 0, 0},
            {"Cadoc", 2, 2400, 2.5f, 13, 82000, 54750, 75, 1750, 18000, 125000, 180, 45, 0},
            {"Bran", 3, 3180, 2.2f, 15, 104000, 91000, 150, 2450, 22000, 190000, 250, 70, 18},
            {"Dagonet", 1, 1760, 2.9f, 10, 62000, 45200, 25, 1100, 14200, 79000, 90, 0, 0},
            {"Edern", 2, 2360, 2.5f, 13, 80000, 80000, 70, 1680, 17600, 118000, 170, 40, 0},
            {"Fintan", 4, 4020, 1.9f, 18, 132000, 118500, 240, 3300, 28000, 0, 0, 0, 0},
            {"Geraint", 1, 1820, 2.7f, 11, 66000, 52500, 30, 1250, 15400, 87000, 130, 0, 0},
            {"Hoel", 2, 2480, 2.4f, 14, 84500, 84500, 90, 1820, 18600, 128000, 185, 42, 0},
            {"Isolde", 3, 3260, 2.1f, 15, 108000, 73000, 160, 2520, 22800, 198000, 270, 75, 20},
            {"Judicael", 2, 2420, 2.5f, 13, 82500, 60100, 75, 1760, 18100, 123000, 176, 44, 0},
            {"Kael", 1, 1740, 3.0f, 10, 61000, 38900, 22, 1020, 13800, 76000, 85, 0, 0},
            {"Maelor", 4, 4100, 1.8f, 18, 136000, 136000, 260, 3460, 29200, 0, 0, 0, 0}
        });
    guildTowerWidget.setSelectedTowerLevelIndex(1);
    guildTowerWidget.setTreasuryGoldAmount(3147765460LL);
    guildTowerWidget.setTreasuryRubiesAmount(396083);
    guildTowerWidget.setTreasuryPearlsAmount(820450);
    guildTowerWidget.setTreasuryCrystalsAmount(40320);
}

void GameScene::populateAccountManagementDemoData(void)
{
    using EliteShipsTabShipEntry = AccountManagementWidget::EliteShipsTabShipEntry;
    using SpecialShipsTabShipEntry = AccountManagementWidget::SpecialShipsTabShipEntry;
    using ShipManagementTabOptionEntry = AccountManagementWidget::ShipManagementTabOptionEntry;
    using AppearanceTabOptionEntry = AccountManagementWidget::AppearanceTabOptionEntry;
    using StorageTabItemEntry = AccountManagementWidget::StorageTabItemEntry;
    using StorageTabEquipmentCategory = AccountManagementWidget::StorageTabEquipmentCategory;
    using StorageTabEquipmentOptionEntry = AccountManagementWidget::StorageTabEquipmentOptionEntry;
    using StorageTabCannonStatsDisplay = AccountManagementWidget::StorageTabCannonStatsDisplay;
    using ForgeTabCannonEntry = AccountManagementWidget::ForgeTabCannonEntry;
    using ForgeTabCannonStatsDisplay = AccountManagementWidget::ForgeTabCannonStatsDisplay;
    using ForgeTabCannonUpgradeEntry = AccountManagementWidget::ForgeTabCannonUpgradeEntry;
    using AccountTabEliteProgressData = AccountManagementWidget::AccountTabEliteProgressData;
    using BoardingLootCurrencyEntry = AccountManagementWidget::BoardingLootCurrencyEntry;
    using BoardingLootCurrencyType = AccountManagementWidget::BoardingLootCurrencyType;

    // Exemple de flux d'alimentation:
    // 1. le gameplay choisit le dossier du navire reel a charger en scene,
    // 2. il injecte ensuite les images d'apercu et les listes dans le widget.
    this->playerShipFolderPath = "assets/images/ships/bateau elite 4";

    GetIngameHudOverlay().getAccountManagementWidget().setAccountTabPlayerIdentifier("1985");
    GetIngameHudOverlay().getAccountManagementWidget().setAccountTabPirateSince("01.02.2024");
    GetIngameHudOverlay().getAccountManagementWidget().setAccountTabPlayerLevel(10);
    GetIngameHudOverlay().getAccountManagementWidget().setAccountTabExperiencePointsCurrent(this->playerExperiencePointsCurrent);
    GetIngameHudOverlay().getAccountManagementWidget().setAccountTabEliteProgressData(
        AccountTabEliteProgressData{
            4250000,
            5000000,
            true
        });
    GetIngameHudOverlay().getAccountManagementWidget().setAccountTabCombatPointsCurrent(1460);
    GetIngameHudOverlay().getAccountManagementWidget().setAccountTabPremiumSince("06.04.2026");
    GetIngameHudOverlay().getAccountManagementWidget().setAccountTabProfileName(".Crows");

    static constexpr const char* kEliteDemoAssetPaths[4] = {
        "assets/images/ships/bateau elite 1",
        "assets/images/ships/bateau elite 2",
        "assets/images/ships/bateau elite 3",
        "assets/images/ships/bateau elite 4"
    };
    static constexpr const char* kSpecialDemoNames[3] = {"Boreas", "Fly dutchman", "Morgan Boucanier"};
    static constexpr const char* kSpecialDemoAssetPaths[3] = {
        "assets/images/ships/Boreas (1)",
        "assets/images/ships/Fly dutchman (1)",
        "assets/images/ships/Morgan Boucanier (1)"
    };

    std::vector<EliteShipsTabShipEntry> eliteDemoShips{
        {"Elite 1", kEliteDemoAssetPaths[0]},
        {"Elite 2", kEliteDemoAssetPaths[1]},
        {"Elite 3", kEliteDemoAssetPaths[2]},
        {"Elite 4", kEliteDemoAssetPaths[3]},
    };
    for (int i = 0; i < 25; ++i)
    {
        eliteDemoShips.push_back(
            {std::string("Elite ") + std::to_string(5 + i), kEliteDemoAssetPaths[static_cast<std::size_t>(i % 4)]});
    }
    GetIngameHudOverlay().getAccountManagementWidget().setEliteShipsTabAcquiredShips(eliteDemoShips);

    std::vector<SpecialShipsTabShipEntry> specialDemoShips{
        {kSpecialDemoNames[0], kSpecialDemoAssetPaths[0]},
        {kSpecialDemoNames[1], kSpecialDemoAssetPaths[1]},
        {kSpecialDemoNames[2], kSpecialDemoAssetPaths[2]},
    };
    for (int i = 0; i < 25; ++i)
    {
        const std::size_t slot = static_cast<std::size_t>(i % 3);
        specialDemoShips.push_back(
            {std::string(kSpecialDemoNames[slot]) + " +" + std::to_string(i + 1), kSpecialDemoAssetPaths[slot]});
    }
    GetIngameHudOverlay().getAccountManagementWidget().setSpecialShipsTabAcquiredShips(specialDemoShips);

    // Icones reparties sur les nombreuses lignes demo (scrollbars pickers 7 lignes max, effets 5).
    static constexpr const char* kDemoPickerIcons[10] = {
        "assets/images/ships/bateau elite 4",
        "assets/images/ships/bateau elite 3",
        "assets/images/ships/bateau elite 2",
        "assets/images/ships/bateau elite 5",
        "assets/images/ships/bateau elite 7",
        "assets/images/ammo/bazar-marche/ammo-rep-icon.png",
        "assets/images/ammo/bazar-marche/ammo-creux-icon.png",
        "assets/images/ammo/bazar-marche/ammo-explo-icon.png",
        "assets/images/ammo/bazar-marche/ammo-illu-icon.png",
        "assets/images/ammo/bazar-marche/ammo-shrapnel-icon.png"};
    static constexpr const char* kDemoVfxSpeedIcons[6] = {
        "assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-sprites/2.png",
        "assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-sprites/6.png",
        "assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-sprites/10.png",
        "assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-sprites/14.png",
        "assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-sprites/20.png",
        "assets/images/vfx/vfx-speedwhitedeux/vfx-speedwhitedeux-sprites/30.png"};
    static constexpr const char* kDemoCannonIcons[4] = {
        "assets/images/cannons/10-livres.png",
        "assets/images/cannons/20-livres.png",
        "assets/images/cannons/30-livres.png",
        "assets/images/cannons/40-livres.png"};

    auto pushNumberedShipBonuses = [&]() {
        std::vector<ShipManagementTabOptionEntry> v{
            {"Bateau elite 4", kDemoPickerIcons[0]},
            {"Bateau elite 3", kDemoPickerIcons[1]},
            {"Bateau elite 2", kDemoPickerIcons[2]}};
        for (int i = 4; i <= 28; ++i)
        {
            v.push_back(
                {std::string("Bonus navire demo ") + std::to_string(i),
                 kDemoPickerIcons[static_cast<std::size_t>(i) % 10U]});
        }
        return v;
    };
    GetIngameHudOverlay().getAccountManagementWidget().setShipManagementTabBonusOptions(pushNumberedShipBonuses());

    auto pushNumberedShipStyles = [&]() {
        std::vector<AppearanceTabOptionEntry> v{
            {"Bateau elite 4", kDemoPickerIcons[0]},
            {"Bateau elite 5", kDemoPickerIcons[3]},
            {"Bateau elite 7", kDemoPickerIcons[4]}};
        for (int i = 4; i <= 28; ++i)
        {
            v.push_back(
                {std::string("Style navire demo ") + std::to_string(i),
                 kDemoPickerIcons[static_cast<std::size_t>(i) % 10U]});
        }
        return v;
    };
    GetIngameHudOverlay().getAccountManagementWidget().setAppearanceTabShipStyleOptions(pushNumberedShipStyles());

    auto pushNumberedRepair = [&]() {
        std::vector<AppearanceTabOptionEntry> v{
            {"Reparation par defaut", kDemoPickerIcons[5]},
            {"Reparation emeraude", kDemoPickerIcons[6]},
            {"Reparation abyssale", kDemoPickerIcons[7]}};
        for (int i = 4; i <= 28; ++i)
        {
            v.push_back(
                {std::string("Reparation variante ") + std::to_string(i),
                 kDemoPickerIcons[static_cast<std::size_t>(i) % 10U]});
        }
        return v;
    };
    GetIngameHudOverlay().getAccountManagementWidget().setAppearanceTabRepairStyleOptions(pushNumberedRepair());

    auto pushNumberedSpeed = [&]() {
        std::vector<AppearanceTabOptionEntry> v{
            {"Vitesse blanche", kDemoVfxSpeedIcons[0]},
            {"Vitesse tempete", kDemoVfxSpeedIcons[2]},
            {"Vitesse neon", kDemoVfxSpeedIcons[5]}};
        for (int i = 4; i <= 28; ++i)
        {
            v.push_back(
                {std::string("Vitesse demo ") + std::to_string(i),
                 kDemoVfxSpeedIcons[static_cast<std::size_t>(i) % 6U]});
        }
        return v;
    };
    GetIngameHudOverlay().getAccountManagementWidget().setAppearanceTabSpeedStyleOptions(pushNumberedSpeed());

    auto pushNumberedImpact = [&]() {
        std::vector<AppearanceTabOptionEntry> v{
            {"Impact standard", kDemoPickerIcons[7]},
            {"Impact royal", kDemoPickerIcons[8]},
            {"Impact titan", kDemoPickerIcons[9]}};
        for (int i = 4; i <= 28; ++i)
        {
            v.push_back(
                {std::string("Impact demo ") + std::to_string(i),
                 kDemoPickerIcons[static_cast<std::size_t>(i) % 10U]});
        }
        return v;
    };
    GetIngameHudOverlay().getAccountManagementWidget().setAppearanceTabProjectileImpactStyleOptions(pushNumberedImpact());

    auto pushNumberedRocket = [&]() {
        std::vector<AppearanceTabOptionEntry> v{
            {"Fusee comete", kDemoPickerIcons[7]},
            {"Fusee oracle", kDemoPickerIcons[6]},
            {"Fusee solaire", kDemoPickerIcons[5]}};
        for (int i = 4; i <= 28; ++i)
        {
            v.push_back(
                {std::string("Fusee demo ") + std::to_string(i),
                 kDemoPickerIcons[static_cast<std::size_t>(i) % 10U]});
        }
        return v;
    };
    GetIngameHudOverlay().getAccountManagementWidget().setAppearanceTabRocketStyleOptions(pushNumberedRocket());

    auto pushNumberedProjectile = [&]() {
        std::vector<AppearanceTabOptionEntry> v{
            {"Boulet lourd", kDemoPickerIcons[7]},
            {"Boulet arc", kDemoPickerIcons[8]},
            {"Boulet obsidienne", kDemoPickerIcons[9]}};
        for (int i = 4; i <= 28; ++i)
        {
            v.push_back(
                {std::string("Projectile demo ") + std::to_string(i),
                 kDemoPickerIcons[static_cast<std::size_t>(i) % 10U]});
        }
        return v;
    };
    GetIngameHudOverlay().getAccountManagementWidget().setAppearanceTabProjectileStyleOptions(pushNumberedProjectile());

    auto pushNumberedMoveClick = [&]() {
        std::vector<AppearanceTabOptionEntry> v{
            {"Clic tempete", kDemoPickerIcons[5]},
            {"Clic royal", kDemoPickerIcons[8]},
            {"Clic aurore", kDemoPickerIcons[6]}};
        for (int i = 4; i <= 28; ++i)
        {
            v.push_back(
                {std::string("Clic deplacement demo ") + std::to_string(i),
                 kDemoPickerIcons[static_cast<std::size_t>(i) % 10U]});
        }
        return v;
    };
    GetIngameHudOverlay().getAccountManagementWidget().setAppearanceTabMoveClickStyleOptions(pushNumberedMoveClick());

    auto pushNumberedEmotes = [&]() {
        std::vector<AppearanceTabOptionEntry> v{
            {"Hello", kDemoPickerIcons[8]},
            {"Attack", kDemoPickerIcons[7]},
            {"Laugh", kDemoPickerIcons[6]},
            {"Lets go", kDemoPickerIcons[9]},
            {"Support", kDemoPickerIcons[5]},
            {"Bravo", kDemoPickerIcons[8]}};
        for (int i = 7; i <= 28; ++i)
        {
            v.push_back(
                {std::string("Emote demo ") + std::to_string(i),
                 kDemoPickerIcons[static_cast<std::size_t>(i) % 10U]});
        }
        return v;
    };
    GetIngameHudOverlay().getAccountManagementWidget().setAppearanceTabEmoteOptions(pushNumberedEmotes());

    auto pushNumberedStorage = [&]() {
        return std::vector<StorageTabEquipmentOptionEntry>{
            {StorageTabEquipmentCategory::CANNONS, "Cannons", kDemoCannonIcons[0], 24},
            {StorageTabEquipmentCategory::SAILS, "Voiles", kDemoVfxSpeedIcons[1], 1}};
    };
    GetIngameHudOverlay().getAccountManagementWidget().setStorageTabEquipmentCategoryOptions(pushNumberedStorage());
    GetIngameHudOverlay().getAccountManagementWidget().setStorageTabWarehouseItems(
        std::vector<StorageTabItemEntry>{
            {StorageTabEquipmentCategory::CANNONS, "Canons 10 livres", kDemoCannonIcons[0], 32, 0, StorageTabCannonStatsDisplay{"+10%", "+2%", "+1%", "+4", "2,4/s"}},
            {StorageTabEquipmentCategory::CANNONS, "Canons 20 livres", kDemoCannonIcons[1], 24, 1, StorageTabCannonStatsDisplay{"+20%", "+4%", "+2%", "+5", "2,1/s"}},
            {StorageTabEquipmentCategory::CANNONS, "Canons 30 livres", kDemoCannonIcons[2], 16, 2, StorageTabCannonStatsDisplay{"+30%", "+7%", "+3%", "+6", "1,8/s"}},
            {StorageTabEquipmentCategory::CANNONS, "Canons 40 livres", kDemoCannonIcons[3], 8, 3, StorageTabCannonStatsDisplay{"+40%", "+10%", "+4%", "+7", "1,5/s"}},
            {StorageTabEquipmentCategory::SAILS, "Voiles tempete", kDemoVfxSpeedIcons[1], 2, 0, StorageTabCannonStatsDisplay{}}});
    GetIngameHudOverlay().getAccountManagementWidget().setStorageTabEquippedItems(
        std::vector<StorageTabItemEntry>{
            {StorageTabEquipmentCategory::CANNONS, "Canons 20 livres", kDemoCannonIcons[1], 12, 1, StorageTabCannonStatsDisplay{"+20%", "+4%", "+2%", "+5", "2,1/s"}},
            {StorageTabEquipmentCategory::SAILS, "Voiles standards", kDemoVfxSpeedIcons[2], 1, 0, StorageTabCannonStatsDisplay{}}});
    GetIngameHudOverlay().getAccountManagementWidget().setForgeTabCannons(
        std::vector<ForgeTabCannonEntry>{
            {
                "Canons 40 livres",
                kDemoCannonIcons[3],
                18,
                0,
                ForgeTabCannonStatsDisplay{"+40%", "+10%", "+4%", "+7", "1,5/s"},
                std::vector<ForgeTabCannonUpgradeEntry>{
                    {1, 4, 12, 2, ForgeTabCannonStatsDisplay{"+44%", "+12%", "+5%", "+8", "1,4/s"}},
                    {2, 8, 20, 4, ForgeTabCannonStatsDisplay{"+49%", "+14%", "+6%", "+8", "1,3/s"}},
                    {3, 14, 34, 7, ForgeTabCannonStatsDisplay{"+55%", "+18%", "+8%", "+9", "1,2/s"}}
                }
            },
            {
                "Canons 50 livres",
                kDemoCannonIcons[2],
                10,
                1,
                ForgeTabCannonStatsDisplay{"+52%", "+13%", "+5%", "+8", "1,4/s"},
                std::vector<ForgeTabCannonUpgradeEntry>{
                    {2, 10, 24, 5, ForgeTabCannonStatsDisplay{"+58%", "+16%", "+7%", "+9", "1,3/s"}},
                    {3, 16, 38, 8, ForgeTabCannonStatsDisplay{"+64%", "+20%", "+9%", "+10", "1,2/s"}}
                }
            },
            {
                "Canons 60 livres",
                kDemoCannonIcons[1],
                6,
                2,
                ForgeTabCannonStatsDisplay{"+65%", "+18%", "+8%", "+10", "1,2/s"},
                std::vector<ForgeTabCannonUpgradeEntry>{
                    {3, 18, 42, 9, ForgeTabCannonStatsDisplay{"+72%", "+22%", "+10%", "+11", "1,1/s"}},
                    {4, 26, 58, 14, ForgeTabCannonStatsDisplay{"+80%", "+27%", "+12%", "+12", "1,0/s"}}
                }
            }
        });
    GetIngameHudOverlay().getAccountManagementWidget().setForgeTabTreasuryRubiesAmount(3500);
    GetIngameHudOverlay().getAccountManagementWidget().setForgeTabTreasuryPearlsAmount(820);
    GetIngameHudOverlay().getAccountManagementWidget().setForgeTabTreasuryCrystalsAmount(40);
    GetIngameHudOverlay().getAccountManagementWidget().setBoardingLootManagementTabCurrencies(
        std::vector<BoardingLootCurrencyEntry>{
            {BoardingLootCurrencyType::GOLD, "assets/images/ui-scene-game/money-gold.png", 1250000, 0, 0, true},
            {BoardingLootCurrencyType::PERLES, "assets/images/ui-scene-game/money_pearls.png", 820, 4500, 50, false},
            {BoardingLootCurrencyType::CRISTAUX, "assets/images/ui-scene-game/money_crystals.png", 40, 320, 50, false}});
}

void GameScene::syncHudStatusWidgets(const Player& player)
{
    GetIngameHudOverlay().getHpBarWidget().setMaxHp(player.getHpMax());
    GetIngameHudOverlay().getHpBarWidget().setCurrentHp(player.getHpCurrent());
    GetIngameHudOverlay().getExperienceBarWidget().setCurrentExperiencePoints(this->playerExperiencePointsCurrent);
}

void GameScene::initializePlayerSpawnAndCamera(void)
{
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    Camera& camera = GetCamera();
    const std::string& shipFolderPath = this->playerShipFolderPath;

    // Load le navire du joueur.
    if (!player.loadShip(shipFolderPath.c_str()))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "GameScene: echec chargement navire '%s'",
            shipFolderPath.c_str());
    }

    // Load le VFX du navire du joueur.
    /*if (!this->shipVfx.loadFromFolders(this->playerShipFolderPath.c_str(), this->playerVfxFolderPath.c_str()))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "GameScene: echec chargement VFX (shipFolder='%s', vfxFolder='%s')",
            this->playerShipFolderPath.c_str(),
            this->playerVfxFolderPath.c_str());
        return;
    }*/

    // Spawn au secteur 30-AE (centre approximatif).
    player.spawnOnSector(map, 30, 30);

    // Centre la camera sur le joueur au debut.
    GameplayCameraController::centerOnPlayer(camera, map, map.rect, player);
    this->shipAutoFollowEnabled = true;
    camera.update(map, map.rect);
}

void GameScene::unload(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Player& player = GetGameState().player;

    // Libere les ressources du jeu.
    GameplayShaderController::unloadAll();
    //this->shipVfx.unload();
    player.unload();
    GetIngameHudOverlay().unload();
}

void GameScene::load(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;

    // Configure le joueur.
    player.load();

    // Charge les shaders de gameplay.
    GameplayShaderController::loadAll();

    // Charge les ressources HUD (interface utilisateur).
    GetIngameHudOverlay().load();
    GetIngameHudOverlay().getCaptchaWidget().publishCaptchaChallenge("A7K9"); // texte serveur
    this->syncHudStatusWidgets(player);
    this->populateMoneyDemoData();
    this->populateAccountManagementDemoData();
    this->populateGuildMortarData();
    this->populateGuildTowerData();

    // Met a jour le rectangle map (zone monde) a partir du game screen.
    map.update();

    // Initialise le spawn joueur + camera de depart.
    this->initializePlayerSpawnAndCamera();

}

void GameScene::update(double dt)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    Camera& camera = GetCamera();
    GameSettingsWidget& gameSettings = GetIngameHudOverlay().getGameSettingsWidget();

    // Met a jour le rectangle map (zone monde) a partir du game screen.
    map.update();

    // Applique les options graphiques qui pilotent les passes animees.
    oceanShader.setWakeTrailsEnabled(gameSettings.getShipWakeTrailsEnabled());

    // Met a jour le shader ocean.
    oceanShader.update(dt);

    // Met a jour le joueur (deplacement, animation, etc).
    // + synchronisation avec le shader ocean pour les effets de wake.
    oceanShader.beginWakeFrame(dt);
    player.update(dt, map);
    oceanShader.endWakeFrame(map, map.rect);

    // Met a jour les VFX du navire du joueur.
    //this->shipVfx.update(dt, player.getShip(), nullptr);

    // Met a jour les shaders de visibilite (nuages + fog).
    GameplayShaderController::updateVisibility(dt, player, gameSettings.getFogOfWarEnabled());

    this->syncHudStatusWidgets(player);

    // Met a jour tout le HUD (widgets + overlays monde).
    GetIngameHudOverlay().update(dt, camera, map);

    // Deplacement camera continu : ignore tant qu'un champ texte HUD a le focus (ZQSD, fleches, etc.).
    if (!GetIngameHudOverlay().isBlockingGameplayKeyboardInput())
    {
        if (GameplayCameraController::updateKeyboardScroll(
                dt,
                camera,
                map,
                map.rect,
                GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CAMERA_MOVE_UP),
                GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CAMERA_MOVE_DOWN),
                GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CAMERA_MOVE_LEFT),
                GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CAMERA_MOVE_RIGHT),
                GetIngameHudOverlay().getGameSettingsWidget().getCameraScrollSpeedSectors()))
        {
            // L'utilisateur prend le controle manuel de la camera.
            this->shipAutoFollowEnabled = false;
        }
    }

    // Tant qu'aucun controle camera manuel n'est utilise,
    // la camera suit en permanence le navire.
    if (this->shipAutoFollowEnabled)
    {
        GameplayCameraController::centerOnPlayer(camera, map, map.rect, player);
    }

    // Applique la camera finale (zoom + position) sur la map.
    camera.update(map, map.rect);
}

void GameScene::draw(void)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    OceanShader& oceanShader = GetOceanShader();
    VisionCloudShader& visionCloudShader = GetVisionCloudShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();
    GameSettingsWidget& gameSettings = GetIngameHudOverlay().getGameSettingsWidget();

    // Dessine le fond UI en premier (coordonnees logiques absolues).
    GetIngameHudOverlay().drawBackgroundWidget();

    // Clip strict du rendu gameplay dans la zone map.
    SDL_Renderer* renderer = WorldRenderClip::begin(map.rect);

    // Dessine l'ocean.
    if (oceanShader.isReady())
    {
        oceanShader.draw(map.rect);
    }

    // Dessine le fog-of-war au-dessus de l'ocean.
    if (gameSettings.getFogOfWarEnabled() && fogOfWarShader.isReady())
    {
        fogOfWarShader.draw(map.rect);
    }

    // Dessine le marqueur de clic.
    GetIngameHudOverlay().drawTileClickMarkerOverlay(map);

    // Dessine les VFX derriere le ship.
    //this->shipVfx.draw(map, player.getShip(), true);

    // Dessine le joueur.
    player.draw(map);

    // Dessine les VFX devant le ship.
    //this->shipVfx.draw(map, player.getShip(), false);

    // Dessine les nuages par-dessus le joueur pour un rendu "au-dessus".
    if (visionCloudShader.isReady())
    {
        visionCloudShader.draw(map.rect);
    }

    // Dessine les barres de scroll par-dessus tout.
    GetIngameHudOverlay().drawScrollBarOverlay(map);

    // Fin du clip monde: l'overlay/UI peut dessiner librement.
    WorldRenderClip::end(renderer);

    // Dessine les elements d'interface.
    GetIngameHudOverlay().drawWidgets(map, player);
}

void GameScene::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    Camera& camera = GetCamera();
    bool cameraChanged = false;

    // Priorite au chat HUD: si la touche est consommee par l'UI, on stop ici.
    if (GetIngameHudOverlay().keypressed(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }

    // Champ texte HUD actif : ne pas declencher recentrage camera / raccourcis lies aux touches.
    if (GetIngameHudOverlay().isBlockingGameplayKeyboardInput())
    {
        return;
    }

    // La touche configuree recentre la camera sur le joueur et reactive le suivi auto.
    if (scancode == GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::CENTER_CAMERA_ON_SHIP))
    {
        GameplayCameraController::centerOnPlayer(camera, map, map.rect, player);
        this->shipAutoFollowEnabled = true;
        cameraChanged = true;
    }

    // Affiche/masque la minimap selon la touche configuree.
    if (!isrepeat &&
        scancode == GetIngameHudOverlay().getGameSettingsWidget().getControlActionScancode(GameSettingsWidget::ControlAction::TOGGLE_MINIMAP))
    {
        GetIngameHudOverlay().toggleHudWidgetVisibility(GameSettingsWidget::HudScaleTarget::MINIMAP);
    }

    // Applique la camera si elle a ete modifiee.
    if (cameraChanged)
    {
        camera.update(map, map.rect);
    }
}

void GameScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    // Recupere les references aux systemes et objets necessaires.
    Map& map = GetCurrentMap();
    Player& player = GetGameState().player;
    Camera& camera = GetCamera();
    IngameHudOverlay& hudOverlay = GetIngameHudOverlay();

    // Priorite au chat HUD: clic consomme => pas de propagation gameplay.
    if (hudOverlay.mousepressed(x, y, button, clicks, mouseID))
    {
        return;
    }

    if (hudOverlay.centerShipButtonMousepressed(x, y, button))
    {
        GameplayCameraController::centerOnPlayer(camera, map, map.rect, player);
        this->shipAutoFollowEnabled = true;
        camera.update(map, map.rect);
        return;
    }

    // Si le clic tombe sur une barre de scroll, on ne le propage pas au reste.
    if (hudOverlay.handleMapOverlayMousePressed(x, y, button, camera, map))
    {
        this->shipAutoFollowEnabled = false;
        return;
    }

    // Seuls les clics gauche sont traites pour le gameplay.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    // Ignore les clics en dehors de la zone map (GUI en haut/bas).
    if (x < map.rect.x || x > (map.rect.x + map.rect.w) ||
        y < map.rect.y || y > (map.rect.y + map.rect.h))
    {
        return;
    }

    // Convertit les coordonnees de clic en coordonnees de tuile.
    const SDL_Point tile = map.screenToTileNearest(x, y);
    // Verifie que la tuile est dans la map et traversable.
    if (!map.isInside(tile.x, tile.y) || map.isTileBlocked(tile.x, tile.y))
    {
        return;
    }
    else
    {
        // Deplace le joueur vers la tuile cliquee.
        player.moveToTile(map, tile.x, tile.y);

        // Affiche le marqueur de clic sur la tuile cliquee.
        GetIngameHudOverlay().notifyMapTileClicked(tile.x, tile.y);
    }
}

void GameScene::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    // Priorite au chat HUD pour la molette (souris + trackpad).
    if (GetIngameHudOverlay().mousewheelmoved(direction, x, y, integer_x, integer_y, mouse_x, mouse_y, mouseID))
    {
        return;
    }
}
