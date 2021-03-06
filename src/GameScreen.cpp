#include "Player.h"
#include "GameData.h"
#include "GameUI.h"
#include "Helpers.h"
#include "GameScreen.h"
#include "Constants.h"

namespace {
	const unsigned int BULLET_POOL_SIZE = 100;
}

Screen::Type GameScreen::run(sf::RenderWindow &window) {
	sf::Event Event;
	bool Running = true;
	sf::Clock frameClock;
	bool pause = false;

	AudioManager audioManager;
	sf::Vector2u windowSize = window.getSize();
	BulletPool bulletPool(BULLET_POOL_SIZE, audioManager);
	Player player(sf::Vector2f(windowSize.x * TILE_SIZE * 2.f, windowSize.y * 0.5f), bulletPool);
	PrisonManager prisonManager(windowSize);
	GameUI gameUI(windowSize);
	bool showLevelSwitchPads = false;
	int level = 0;
	audioManager.play(AudioManager::Type::Select);
	audioManager.play(AudioManager::Type::GameMusic);

	struct LevelSwitchPad {
		sf::Vector2f position;
		sf::Sprite sprite;
		sf::Vector2f triggerPosition;
		float triggerRadius;
	};
	const int LevelSwitchPadSize = 3;
	LevelSwitchPad levelSwitchPads[LevelSwitchPadSize];
	for (int i = 0; i < LevelSwitchPadSize; i++) {
		levelSwitchPads[i].position.x = windowSize.x - (TILE_SIZE * 2.f * windowSize.x);
		levelSwitchPads[i].sprite.setTexture(GameData::getInstance().switchPadTexture);
		levelSwitchPads[i].sprite.setScale(GameData::getInstance().textureScaler, GameData::getInstance().textureScaler);
		levelSwitchPads[i].sprite.setOrigin(levelSwitchPads[i].sprite.getGlobalBounds().width * 0.5f, levelSwitchPads[i].sprite.getGlobalBounds().height * 0.5f);
		levelSwitchPads[i].triggerRadius = TILE_SIZE * 1.5f * windowSize.x;
		levelSwitchPads[i].sprite.setPosition(levelSwitchPads[i].position);
	}
	//so messy 
	levelSwitchPads[0].position.y = TILE_SIZE * 3.5f * windowSize.x;
	levelSwitchPads[0].sprite.setPosition(levelSwitchPads[0].position);
	levelSwitchPads[0].triggerPosition = levelSwitchPads[0].position + sf::Vector2f(TILE_SIZE * 2.2f * windowSize.x, 0.f);
	levelSwitchPads[1].position.y = TILE_SIZE * 11.f * windowSize.x;
	levelSwitchPads[1].sprite.setPosition(levelSwitchPads[1].position);
	levelSwitchPads[1].triggerPosition = levelSwitchPads[1].position + sf::Vector2f(TILE_SIZE * 2.2f * windowSize.x, 0.f);
	levelSwitchPads[2].position.y = TILE_SIZE * 18.5f * windowSize.x;
	levelSwitchPads[2].sprite.setPosition(levelSwitchPads[2].position);
	levelSwitchPads[2].triggerPosition = levelSwitchPads[2].position + sf::Vector2f(TILE_SIZE * 2.2f * windowSize.x, 0.f);

	float m_powerDecreasePerSecond = POWER_DRAIN_PER_SECOND;
	sf::Sprite background(GameData::getInstance().backgroundTexture);
	background.setScale(windowSize.x / background.getLocalBounds().width, windowSize.y / background.getLocalBounds().height);
	
	while (Running)	{
		while (window.pollEvent(Event)) {
			if (Event.type == sf::Event::Closed || 
				(Event.type == sf::Event::KeyPressed && Event.key.code == sf::Keyboard::Escape)) {
				return Screen::Type::Exit;
			}
			if (Event.type == sf::Event::LostFocus) {
				pause = true;
			}
			if (Event.type == sf::Event::GainedFocus) {
				pause = false;
			}
			if (Event.type == sf::Event::MouseButtonPressed &&
				Event.mouseButton.button == sf::Mouse::Button::Left) {
				player.setFiring(true);
			}
		}		

		//update game screen
		float dt = frameClock.restart().asSeconds();
		if (dt > 0.3f || pause)
			dt = 0.f;	

		sf::Vector2f direction = (sf::Vector2f)sf::Mouse::getPosition(window) - player.getPosition();
		player.setDirection(direction);
		player.update(dt);
		bulletPool.update(dt);
		int numOfCollision = prisonManager.update(dt, player.getPosition(), player.getSize());
		if (numOfCollision > 0) {
			if (gameUI.changeHealth(-numOfCollision) == false) {
				audioManager.play(AudioManager::Type::Death);
				GameData::getInstance().levelReached = level;
				return Screen::Type::GameOver;
			}
			audioManager.play(AudioManager::Type::Hit);
		}
		int prisonersRemaining = prisonManager.getPrisonersRemaining();
		int maxPrisoners = prisonManager.getMaxPrisoners();
		gameUI.setPrisonerInfo(prisonersRemaining, maxPrisoners);

		float previousPower = gameUI.getPower();
		bool openExit = !gameUI.changePower(-POWER_DRAIN_PER_SECOND * dt);
		if (openExit) {
			prisonManager.decreasePower();
			showLevelSwitchPads = true;
		}
		else {
			bool decrease = false;
			for (int i = 1; i < MAX_POWER + 1; i++) {
				if (gameUI.getPower() < gameUI.getMaxPower() - (i * POWER_POINTS_PER_LEVEL)) {
					if (previousPower > gameUI.getMaxPower() - (i * POWER_POINTS_PER_LEVEL)) {
						decrease = true;
						break;
					}
				}
			}
			if (decrease) {
				prisonManager.decreasePower();
				gameUI.decreasePowerLevel();
				audioManager.play(AudioManager::Type::CellDoor);
			}
		}

		//restrict to bounds of the play area
		float boundsSize = TILE_SIZE * windowSize.x;
		restrictToBounds(player, boundsSize, windowSize);
		std::vector<Bullet>& bullets = bulletPool.getBullets();
		for (int i = 0; i < bullets.size(); i++) {
			if (bullets[i].getActive()) {
				if (restrictToBounds(bullets[i], boundsSize, windowSize)) {
					bullets[i].setActive(false);
				}
			}
		}

		checkCollisions(bullets, prisonManager.getPrisoners(), audioManager);

		//draw game screen
		window.clear(sf::Color(96, 23, 54));

		window.draw(background);

		if (showLevelSwitchPads && prisonersRemaining == 0) {
			gameUI.decreasePowerLevel();
			for (int i = 0; i < LevelSwitchPadSize; i++) {
				window.draw(levelSwitchPads[i].sprite);

				//also check player collisions with them
				if (circleCollision(player.getPosition(), levelSwitchPads[i].triggerPosition, levelSwitchPads[i].triggerRadius + player.getSize().x)) {
					level++;
					prisonManager.newLevel();
					player.reset();
					bulletPool.reset();
					showLevelSwitchPads = false;
					gameUI.reset(level);
					m_powerDecreasePerSecond += POWER_CHANGE_PER_LEVEL;
					if (m_powerDecreasePerSecond > POWER_DRAIN_MAX)
						m_powerDecreasePerSecond = POWER_DRAIN_MAX;
				}
			}
		}

		window.draw(player);
		window.draw(bulletPool);
		window.draw(prisonManager);
		window.draw(gameUI);


		window.display();
	}

	return Screen::Type::GameOver;
}

void GameScreen::checkCollisions(std::vector<Bullet>& bullets, std::vector<std::unique_ptr<Prisoner>>& prisoners, AudioManager& audioManager) {
	for (int i = 0; i < bullets.size(); i++) {
		if (bullets[i].getActive()) {
			for (int j = 0; j < prisoners.size(); j++) {
				float minDistance = prisoners[j]->getSize().x * 0.5f;
				if (circleCollision(bullets[i].getPosition(), prisoners[j]->getPosition(), minDistance)) {
					bullets[i].setActive(false);
					int healthRemaining = prisoners[j]->decreaseHealth();
					if (healthRemaining == 0) 
						audioManager.play(AudioManager::Type::Death);
					else
						audioManager.play(AudioManager::Type::Hit);
				}
			}
		}
	}

}

bool GameScreen::restrictToBounds(GameObject & object, float boundsSize, const sf::Vector2u& windowSize) {
	bool restricted = false;
	sf::Vector2f objectHalfSize = object.getSize() * 0.5f;
	if (object.getPosition().x - objectHalfSize.x < boundsSize) {
		object.setPositionX(boundsSize + objectHalfSize.x);
		restricted = true;
	}
	else if (object.getPosition().x + objectHalfSize.x > windowSize.x - boundsSize) {
		object.setPositionX(windowSize.x - boundsSize - objectHalfSize.x);
		restricted = true;
	}
	if (object.getPosition().y - objectHalfSize.y < boundsSize) {
		object.setPositionY(boundsSize + objectHalfSize.y);
		restricted = true;
	}
	else if (object.getPosition().y + objectHalfSize.y > windowSize.y - boundsSize) {
		object.setPositionY(windowSize.y - boundsSize - objectHalfSize.y);
		restricted = true;
	}
	return restricted;
}

bool GameScreen::circleCollision(const sf::Vector2f & pos1, const sf::Vector2f & pos2, float minDistance) {
	bool collided = false;
	float distanceBetween = Helpers::getLength(pos2 - pos1);
	if (distanceBetween < minDistance)
		collided = true;
	return collided;
}
