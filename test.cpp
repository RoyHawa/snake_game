#include<iostream>
#include <GL/glut.h>
#include <vector>
#include<thread>
#include <random>
#include <cmath>
#include <queue>
#include <list>
#include <fstream>
#include<string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include<cstdlib>
#include<windows.h> 


std::string playerName;
int score = 0;
int level = 1;

std::thread collisionThread;
std::thread foodAndObstaclesThread;
std::thread moveSnakeThread;

struct Button {
    float x, y, width, height;
    std::string label;

    Button(float x, float y, float width, float height, const std::string& label)
        : x(x), y(y), width(width), height(height), label(label) {}
};
std::vector<Button> welcomeScreenButtons, gameOverButtons, leaderboardScreenButtons;


// Constants for the window size
const int WIDTH = 800;
const int HEIGHT = 600;

// Constants for the snake's initial position and size
const float INITIAL_X = 0.0;
const float INITIAL_Y = 0.0;
const float SQUARE_SIZE = 0.1;

// Variables to store the snake's position and direction
float snakeX = INITIAL_X;
float snakeY = INITIAL_Y;
float directionX;
float directionY;



enum  GameState {
    WELCOME_SCREEN,
    IN_GAME,
    GAME_OVER,
    LEADERBOARD_SCREEN
};

GameState gameState = WELCOME_SCREEN;
bool pause = false;
bool mute = false;

float snakeBodyX, snakeBodyY;

int INITIAL_TIMER_INTERVAL = 300;
int timerInterval = INITIAL_TIMER_INTERVAL;
bool input = false;
const float tolerance = 0.001;

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> randomTimer(5000, 10000);//5s->10s
std::uniform_real_distribution<float> positionDistribution(-0.8, 0.8);

struct Segment {
    float x, y;
    Segment(float _x, float _y) : x(_x), y(_y) {}

};

std::list<Segment> snakeBody; // linked list to store the snake's body
std::vector<Segment> food; // Vector to store the food's positions
std::vector<Segment> obstacles; // Vector to store the obstacles' positions


void saveScore() {
    std::ofstream file("leaderboard.txt", std::ios::app);
    if (file.is_open()) {
        // Get the current date and time
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);

        // Convert the date to a string in "MM/DD/YYYY" format
        std::ostringstream dateStream;
        std::tm tm;
        localtime_s(&tm, &time);
        dateStream << std::put_time(&tm, "%m/%d/%Y");
        std::string date = dateStream.str();

        // Convert the time to a string in "HH:MMam/pm" format
        std::ostringstream timeStream;
        timeStream << std::put_time(&tm, "%I:%M%p");
        std::string timeStr = timeStream.str();

        // Write the name, score, date, and time to the file
        file << playerName << "," << date << "," << timeStr << "," << score << std::endl;
        file.close();
        std::cout << "score saved" << std::endl;
    }
    else {
        std::cerr << "Error: Unable to save score." << std::endl;
    }
}

struct savedScore {
    std::string name;
    int score;
    std::string date;
    std::string time;
};

struct ComparePlayers {
    bool operator()(const savedScore& p1, const savedScore& p2) const {
        if (p1.score != p2.score) {
            return p1.score < p2.score;
        }
        else if (p1.date != p2.date) {
            // Convert date strings to time_t and compare
            std::tm tm1 = {}, tm2 = {};
            std::istringstream ss1(p1.date);
            std::istringstream ss2(p2.date);
            ss1 >> std::get_time(&tm1, "%m/%d/%Y");
            ss2 >> std::get_time(&tm2, "%m/%d/%Y");
            auto time1 = std::mktime(&tm1);
            auto time2 = std::mktime(&tm2);
            return time1 > time2;
        }
        else {
            // Convert time strings to time_t and compare
            std::tm tm1 = {}, tm2 = {};
            std::istringstream ss1(p1.time);
            std::istringstream ss2(p2.time);
            ss1 >> std::get_time(&tm1, "%I:%M%p");
            ss2 >> std::get_time(&tm2, "%I:%M%p");
            if (p1.time.find("PM") != std::string::npos && tm1.tm_hour != 12) tm1.tm_hour += 12;
            if (p2.time.find("PM") != std::string::npos && tm2.tm_hour != 12) tm2.tm_hour += 12;
            if (p1.time.find("AM") != std::string::npos && tm1.tm_hour == 12) tm1.tm_hour = 0;
            if (p2.time.find("AM") != std::string::npos && tm2.tm_hour == 12) tm2.tm_hour = 0;
            int minutes1 = tm1.tm_hour * 60 + tm1.tm_min;
            int minutes2 = tm2.tm_hour * 60 + tm2.tm_min;
            return minutes1 > minutes2;
        }
    }
};


std::priority_queue<savedScore, std::vector<savedScore>, ComparePlayers> leaderboardQueue;
void loadLeaderboard() {
    std::ifstream file("leaderboard.txt");
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string name;
        int score;
        std::string date;
        std::string time;
        std::getline(iss, name, ',');
        std::getline(iss, date, ',');
        std::getline(iss, time, ',');
        iss >> score;

        std::cout << "date: " << date << std::endl;
        std::cout << "time: " << time << std::endl;
        leaderboardQueue.push(savedScore{ name, score, date, time });
    }
}

void playSound(int type) {
    if (!mute) {
        if (type == 1) {
            PlaySound(TEXT("start.wav"), NULL, SND_FILENAME | SND_ASYNC);
        }
        else if (type == 2) {
            PlaySound(TEXT("eat.wav"), NULL, SND_FILENAME | SND_ASYNC);
        }
        else if (type == 3) {
            PlaySound(TEXT("gameover.wav"), NULL, SND_FILENAME | SND_ASYNC);
        }
    }
}


bool isMouseInsideButton(int x, int y, const Button& button) {
    // Convert screen coordinates to normalized coordinates between -1.0 and 1.0
    float normalizedMouseX = (static_cast<float>(x) / WIDTH) * 2.0f - 1.0f;
    float normalizedMouseY = 1.0f - (static_cast<float>(y) / HEIGHT) * 2.0f;

    // Check if the mouse coordinates are inside the button's boundaries
    return (normalizedMouseX >= button.x &&
        normalizedMouseX <= button.x + button.width &&
        normalizedMouseY >= button.y &&
        normalizedMouseY <= button.y + button.height);
}

void drawPauseBars() {
    glColor3f(0.8, 0.8, 0.8);

    // Draw the first bar
    glBegin(GL_QUADS);
    glVertex2f(-0.05, 0.2);
    glVertex2f(-0.1, 0.2);
    glVertex2f(-0.1, -0.2);
    glVertex2f(-0.05, -0.2);
    glEnd();

    // Draw the second bar
    glBegin(GL_QUADS);
    glVertex2f(0.05, 0.2);
    glVertex2f(0.1, 0.2);
    glVertex2f(0.1, -0.2);
    glVertex2f(0.05, -0.2);
    glEnd();
}


void drawSquare(float x, float y, float size) {
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + size, y);
    glVertex2f(x + size, y + size);
    glVertex2f(x, y + size);
    glEnd();
}


void drawButton(const Button& button) {
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(button.x, button.y);
    glVertex2f(button.x + button.width, button.y);
    glVertex2f(button.x + button.width, button.y + button.height);
    glVertex2f(button.x, button.y + button.height);
    glEnd();

    glColor3f(0.0f, 1.0f, 0.0f);
    glRasterPos2f(button.x + button.width / 4, button.y + button.height / 2);
    for (char c : button.label) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
}


void display() {
    glClear(GL_COLOR_BUFFER_BIT);


    glColor3f(0.6, 0.4, 0.2);

    glBegin(GL_LINE_LOOP);
    glVertex2f(-0.91, -0.91);
    glVertex2f(-0.91, 0.91);
    glVertex2f(0.91, 0.91);
    glVertex2f(0.91, -0.91);
    glEnd();


    glColor3f(0.0, 0.3, 0.0);
    glBegin(GL_QUADS);
    glVertex2f(-0.9, -0.9);
    glVertex2f(-0.9, 0.9);
    glVertex2f(0.9, 0.9);
    glVertex2f(0.9, -0.9);
    glEnd();

    glColor3f(1.0, 1.0, 1.0);
    glRasterPos2f(0.1, 0.92);
    std::string scoreText = "Score: " + std::to_string(score);
    for (char c : scoreText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    glRasterPos2f(-0.2, 0.92);
    std::string levelText = "Level: " + std::to_string(level);
    for (char c : levelText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    glColor3f(1.0, 0.0, 0.0);
    glRasterPos2f(0.7, 0.92);
    std::string sound = "Sound ";
    if (mute) {
        sound += "off";
    }
    else {
        sound += "on";
    }
    for (char c : sound) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }


    glColor3f(0.0, 1.0, 0.0);

    for (auto it = snakeBody.begin(); it != snakeBody.end(); ++it) {
        const auto& segment = *it;
        drawSquare(segment.x, segment.y, SQUARE_SIZE);
    }

    glColor3f(0.0, 0.0, 1.0);
    drawSquare(snakeX, snakeY, SQUARE_SIZE);


    glColor3f(1.0, 0.0, 0.0);
    for (const auto& segment : food) {
        drawSquare(segment.x, segment.y, SQUARE_SIZE);
    }

    glColor3f(1.0, 0.0, 1.0);
    for (const auto& segment : obstacles) {
        drawSquare(segment.x, segment.y, SQUARE_SIZE);
    }


    if (pause) {
        drawPauseBars();
    }

    glutSwapBuffers();
}

void displayWelcomeScreen() {
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(0.0, 1.0, 0.0);
    glRasterPos2f(-0.2, 0.9);
    std::string titleText = "Snake Game";
    for (char c : titleText) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
    }


    glColor3f(1.0, 1.0, 1.0);
    float instructionX = -0.4, instructionY = 0.8;
    std::string instructions[5] = { "- Use arrow keys to move." ,"- Eat red fruits to grow." , "- Avoid purple obstacles!" ,"- Level up by eating 3 fruits.","- space to toggle pause/m to toggle sound"};
    for (int i = 0; i < 5; i++) {
        glRasterPos2f(instructionX, instructionY);

        for (char c : instructions[i]) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
        }
        instructionY -= 0.1;
    }

    glColor3f(1.0, 1.0, 1.0);
    glRasterPos2f(-0.3, 0.2);
    std::string nameText = "Name: ";
    for (char c : nameText) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
    }


    glColor3f(1.0, 1.0, 1.0);
    glRasterPos2f(-0.11, 0.2);
    for (size_t i = 0; i < playerName.length(); ++i) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, playerName[i]);
    }


    glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, '|');


    for (const Button& button : welcomeScreenButtons) {
        drawButton(button);
    }
    glutSwapBuffers();
}

void displayGameOverScreen() {
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(1.0, 0.0, 0.0);
    glRasterPos2f(-0.2, 0.8);
    std::string gameOverText = "GAME OVER";
    for (char c : gameOverText) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
    }

    glColor3f(1.0, 0.0, 0.0);
    glRasterPos2f(-0.1, 0.2);

    std::string scoreText = "Score: " + std::to_string(score);
    for (char c : scoreText) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
    }

    glRasterPos2f(-0.1, 0.3);
    std::string levelText = "Level: " + std::to_string(level);
    for (char c : levelText) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
    }

    for (const Button& button : gameOverButtons) {
        drawButton(button);
    }
    glutSwapBuffers();
}


void displayLeaderboard() {
    glClear(GL_COLOR_BUFFER_BIT);

    loadLeaderboard();

    float xPos = -0.6;
    float yPos = 0.7;

    glColor3f(0.0, 1.0, 0.0);
    std::string leaderboardText = "Leaderboard";
    glRasterPos2f(-0.2, 0.9);
    for (char c : leaderboardText) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
    }



    glColor3f(1.0, 1.0, 1.0);

    int counter = 1;
    while (!leaderboardQueue.empty() && counter < 11) {

        savedScore topScore = leaderboardQueue.top();
        int score = topScore.score;
        std::string name = std::to_string(counter) + ".  " + topScore.name;

        std::string scoreText = std::to_string(score);
        std::string date = topScore.date;
        std::string time = topScore.time;
        std::cout << "time: " << time << std::endl;

        glRasterPos2f(xPos, yPos);
        for (char c : name) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
        }

        glRasterPos2f(xPos + 0.4, yPos);
        for (char c : scoreText) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
        }

        glRasterPos2f(xPos + 0.6, yPos);
        for (char c : date) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
        }


        glRasterPos2f(xPos + 0.9, yPos);
        for (char c : time) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
        }

        yPos -= 0.1;

        leaderboardQueue.pop();
        counter++;
    }


    drawButton(leaderboardScreenButtons.at(0));
    glutSwapBuffers();
}

void update(int) {
    if (gameState != GAME_OVER) {
        snakeBodyX = snakeX;
        snakeBodyY = snakeY;

        snakeX += directionX * SQUARE_SIZE;
        snakeY += directionY * SQUARE_SIZE;


        snakeBody.push_front(Segment(snakeBodyX, snakeBodyY));
        snakeBody.pop_back();


        glutPostRedisplay();
        if (snakeX <= -1.0 || snakeX >= 0.9 || snakeY <= -1.0 || snakeY >= 0.9) {
            std::cout << "snake outside borders" << std::endl;
            gameState = GAME_OVER;
            playSound(3);

        }
    }
    else { std::cout << "game over " << std::endl; }
}



float previousX;

void handleKeypress(int key, int, int) {

    if (gameState == IN_GAME && !pause) {
        previousX = directionX;

        switch (key) {
        case GLUT_KEY_UP:
            if (directionY != -1.0) { // Avoid moving into opposite direction
                directionX = 0.0;
                directionY = 1.0; // Up
            }
            break;
        case GLUT_KEY_DOWN:
            if (directionY != 1.0) {
                directionX = 0.0;
                directionY = -1.0; // Down
            }
            break;
        case GLUT_KEY_LEFT:
            if (directionX != 1.0) {
                directionX = -1.0; // Left
                directionY = 0.0;
            }
            break;
        case GLUT_KEY_RIGHT:
            if (directionX != -1.0) {
                directionX = 1.0; // Right
                directionY = 0.0;
            }
            break;
        }
        if (directionX != previousX) {
            input = true;
            update(0);
        }

    }

}






bool collisionWithItem(std::vector<Segment>& items, int type) {
    bool collision = false;

    auto item = items.begin();
    while (item != items.end()) {
        if (std::abs(snakeX - item->x) < tolerance && std::abs(snakeY - item->y) < tolerance) {
            //if (snakeX==(*item).x && snakeY==(*item).y) {not equal due to precision
            if (type) {

                item = items.erase(item);
            }
            std::cout << "collision with item" << std::endl;
            return true;
        }
        else {
            ++item;
        }
    }


    for (const auto& segment : snakeBody) {
        item = items.begin();
        while (item != items.end()) {
            if (std::abs(segment.x - item->x) < tolerance && std::abs(segment.y - item->y) < tolerance) {
                //if (snakeX==(*item).x && snakeY==(*item).y) {not equal due to precision
                if (type) {

                    item = items.erase(item);
                }
                std::cout << "collision with item" << std::endl;
                return true;
            }
            else {
                ++item;
            }
        }
    }

    return false;
}


bool obstacleCollision;
bool foodCollision;
void collisionDetection(int) {

    if (gameState == IN_GAME && !pause) {

        for (const auto& segment : snakeBody) {
            //if (std::abs(snakeX - segment.x) < tolerance && std::abs(snakeY - segment.y) < tolerance) {
            if (snakeX == segment.x && snakeY == segment.y) {
                std::cout << "snake head collision with body" << std::endl;
                playSound(3);
                gameState = GAME_OVER;
                glutPostRedisplay();
                break;
            }
        }

        // snakeMutexLock.unlock();
        if (gameState != GAME_OVER) {


            obstacleCollision = collisionWithItem(obstacles, 0);
            if (obstacleCollision) {
                std::cout << "obstacle collision game over" << std::endl;
                playSound(3);
                gameState = GAME_OVER;
                glutPostRedisplay();
            }
            foodCollision = collisionWithItem(food, 1);
            if (foodCollision) {
                playSound(2);
                //snakeMutexLock.lock();
                score += 1;
                if (score % 3 == 0) {
                    level++;
                    timerInterval -= 30;
                }
                auto lastElementIterator = std::prev(snakeBody.end());
                auto secondToLastElementIterator = std::prev(lastElementIterator); // Iterator to the second-to-last element
                Segment lastSnakeSegment = *lastElementIterator; // Last element
                Segment secondToLastSnakeSegment = *secondToLastElementIterator; // Last element

                float tailX = lastSnakeSegment.x;
                float tailY = lastSnakeSegment.y;
                if (std::abs(lastSnakeSegment.x - secondToLastSnakeSegment.x) < tolerance) {
                    if (directionY == 1.0) {
                        tailY -= 0.1;
                    }
                    else {
                        tailY += 0.1;
                    }
                }
                else {
                    if (directionX == -1.0) {
                        tailX += 0.1;
                    }
                    else {
                        tailX -= 0.1;
                    }

                }

                snakeBody.push_back(Segment(tailX, tailY));
                //snakeMutexLock.unlock();
                glutPostRedisplay();
            }
            glutTimerFunc(50, collisionDetection, 0);
        }
        else {
            collisionDetection(0);
        }
    }
    else {
        saveScore();
    }
}

int randomNumber, randomTime = 0;
float randomX, randomY;
int itemElapsedTime = 0;
int itemTimerSleepInterval = 500;
void generateFoodOrObstacles(int) {
    if (gameState == IN_GAME && !pause) {

        if (itemElapsedTime < randomTime) {
            itemElapsedTime += itemTimerSleepInterval;
        }
        else {
            randomX = positionDistribution(gen);
            randomX = std::round(randomX * 10.0f) / 10.0f;
            randomY = positionDistribution(gen);
            randomY = std::round(randomY * 10.0f) / 10.0f;

            srand(time(NULL));

            randomNumber = rand() % (level + 2);
            if (randomNumber > 1) {

                obstacles.push_back(Segment(randomX, randomY));
            }
            else {

                food.push_back(Segment(randomX, randomY));

            }
            itemElapsedTime = 0;
            randomTime = randomTimer(gen);
        }
        glutTimerFunc(itemTimerSleepInterval, generateFoodOrObstacles, 0);
    }

}

int elapsedTimer = 0;
int sleepInterval = 10;
void moveSnake(int) {
    if (gameState == IN_GAME && !pause) {
        if (!input) {
            if (elapsedTimer < timerInterval) {
                elapsedTimer += sleepInterval;
            }
            else {
                update(0);
                elapsedTimer = 0;
            }
        }
        else {

            elapsedTimer = 0;
            input = false;
        }
        glutTimerFunc(sleepInterval, moveSnake, 0);
    }

}


void initializeThreads() {
    if (!collisionThread.joinable()) {
        collisionThread = std::thread(collisionDetection, 0);
    }
    if (!foodAndObstaclesThread.joinable()) {
        foodAndObstaclesThread = std::thread(generateFoodOrObstacles, 0);
    }
    if (!moveSnakeThread.joinable()) {
        moveSnakeThread = std::thread(moveSnake, 0);
    }
}

void joinThreads() {
    if (collisionThread.joinable()) {
        collisionThread.join();
    }
    if (foodAndObstaclesThread.joinable()) {
        foodAndObstaclesThread.join();
    }
    if (moveSnakeThread.joinable()) {
        moveSnakeThread.join();
    }

}


void startGame() {
    if (!playerName.empty()) {
        joinThreads();
        playSound(1);
        pause = false;
        level = 1;
        timerInterval = INITIAL_TIMER_INTERVAL;
        snakeX = INITIAL_X;
        snakeY = INITIAL_Y;
        directionX = 1.0;
        directionY = 0.0;
        score = 0;
        snakeBody.clear();
        food.clear();
        obstacles.clear();
        elapsedTimer = 0;
        itemElapsedTime = 0;
        randomTime = 0;

        snakeBody.push_back(Segment(snakeX - 0.1, snakeY));
        snakeBody.push_back(Segment(snakeX - 0.2, snakeY));
        snakeBody.push_back(Segment(snakeX - 0.3, snakeY));
        snakeBody.push_back(Segment(snakeX - 0.4, snakeY));
        gameState = IN_GAME;
        initializeThreads();
    }

}

void handleInput(unsigned char key, int x, int y) {
    if (key == 77 || key == 109) {
        mute = !mute;
        glutPostRedisplay();
    }
    else {
        if (gameState == WELCOME_SCREEN) {
            if (key == 13) { // Enter key
                std::cout << "Player Name: " << playerName << std::endl;
                if (!playerName.empty()) {
                    startGame();
                }
            }
            if (key == 8 && playerName.length() > 0) { // Backspace key
                playerName.pop_back();
            }
            else if (key != 44 && key >= 32 && key <= 126 && playerName.size() <= 20) { // ASCII characters
                std::cout << "key pressed: " << char(key) << std::endl;
                playerName += key;
            }
            glutPostRedisplay();
        }
        else if (gameState == IN_GAME) {
            if (key == 32) {//space
                pause = !pause;
                if (pause) {
                    joinThreads();
                }
                if (!pause) {
                    initializeThreads();
                }
                glutPostRedisplay();
            }
        }
    }
}



void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (gameState == WELCOME_SCREEN) {
            if (isMouseInsideButton(x, y, welcomeScreenButtons.at(0))) {
                startGame();
            }
            else if (isMouseInsideButton(x, y, welcomeScreenButtons.at(1))) {
                std::cout << "leaderboard button pressed" << std::endl;
                gameState = LEADERBOARD_SCREEN;
                glutPostRedisplay();
            }
            else if (isMouseInsideButton(x, y, welcomeScreenButtons.at(2))) {
                joinThreads();
                exit(0);
            }
            else {
                glutPostRedisplay();
            }
        }
        else if (gameState == GAME_OVER) {
            if (isMouseInsideButton(x, y, gameOverButtons.at(0))) {// play again button
                gameState = WELCOME_SCREEN;
                glutPostRedisplay();
            }
            else if (isMouseInsideButton(x, y, gameOverButtons.at(1))) {
                joinThreads();
                exit(0);
            }
        }
        else if (gameState == LEADERBOARD_SCREEN) {
            if (isMouseInsideButton(x, y, leaderboardScreenButtons.at(0))) {// back button
                gameState = WELCOME_SCREEN;
                glutPostRedisplay();
            }

        }
    }
}


void displayController() {
    switch (gameState) {
    case WELCOME_SCREEN:
        displayWelcomeScreen();
        break;
    case IN_GAME:
        display();
        break;
    case GAME_OVER:
        displayGameOverScreen();
        break;
    case LEADERBOARD_SCREEN:
        displayLeaderboard();
        break;
    }
}



int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Snake Game");
    welcomeScreenButtons.push_back(Button(-0.2, -0.1, 0.4, 0.2, "Start Game"));
    welcomeScreenButtons.push_back(Button(-0.2, -0.4, 0.4, 0.2, "Leaderboard"));
    welcomeScreenButtons.push_back(Button(-0.2, -0.7, 0.4, 0.2, "Exit"));
    gameOverButtons.push_back(Button(-0.2, -0.1, 0.4, 0.2, "Play Again"));
    gameOverButtons.push_back(Button(-0.15, -0.4, 0.25, 0.18, "Exit"));
    leaderboardScreenButtons.push_back(Button(-0.2, -0.8, 0.3, 0.2, "Back"));
    glutDisplayFunc(displayController);
    glutSpecialFunc(handleKeypress);
    glutKeyboardFunc(handleInput);
    glutMouseFunc(mouse);
    glutMainLoop();

    return 0;
}

