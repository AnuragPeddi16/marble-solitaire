#include <iostream>
#include <vector>
#include <stack>
#include <cmath>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "file_utils.h"
#include "math_utils.h"

using namespace std;


// GLOBAL CONSTANTS

#define BOARD_SIZE 7 // 7 is default size
enum CellState { INVALID, EMPTY, MARBLE };

char programTitle[] = "Marble Solitaire";
#define WINDOW_WIDTH 1000
# define WINDOW_HEIGHT 1000

const char *pVSFileName = "shaders/shader.vs";
const char *pFSFileName = "shaders/shader.fs";

// Number of triangles for circle approximation
#define CIRCLE_SEGMENTS 30

// Toggle for whether last marble has to be in center to win
#define CENTERWIN false



// GLOBAL VARIABLES

GLuint squareVAO, squareVBO;
GLuint circleVAO, circleVBO;
GLuint gWorldLocation, gColorLocation;

// Game board state
vector<vector<CellState>> board(BOARD_SIZE, vector<CellState>(BOARD_SIZE, INVALID));
stack<vector<vector<CellState>>> moveHistory; // undo
stack<vector<vector<CellState>>> redoHistory; // redo

int selectedRow = -1, selectedCol = -1;

// Number of Undos allowed
int undosRemaining = 3;

// Game win/lose states
bool gameOver = false;
bool gameWon = false;
float gameEndTime = 0.0f;
bool gameLost = false;




// OPENGL STUFF

static void CreateSquareVertexBuffer()
{
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,  // bottom left
         0.5f, -0.5f, 0.0f,  // bottom right
        -0.5f,  0.5f, 0.0f,  // top left

         0.5f, -0.5f, 0.0f,  // bottom right
        -0.5f,  0.5f, 0.0f,  // top left
         0.5f,  0.5f, 0.0f   // top right
    };
    glGenVertexArrays(1, &squareVAO);
    glGenBuffers(1, &squareVBO);
    glBindVertexArray(squareVAO);
    glBindBuffer(GL_ARRAY_BUFFER, squareVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}


static void CreateCircleVertexBuffer()
{
    vector<float> vertices;
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    
    // Generate circle perimeter vertices
    for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
        float angle = 2.0f * 3.1415926f * float(i) / float(CIRCLE_SEGMENTS);
        float x = 0.5f * cosf(angle);
        float y = 0.5f * sinf(angle);
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(0.0f);
    }
    
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);
    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

static void AddShader(GLuint ShaderProgram, const char *pShaderText, GLenum ShaderType)
{
    GLuint ShaderObj = glCreateShader(ShaderType);
    if (ShaderObj == 0) {
        fprintf(stderr, "Error creating shader type %d\n", ShaderType);
        exit(1);
    }
    const GLchar *p[1];
    p[0] = pShaderText;
    GLint Lengths[1];
    Lengths[0] = strlen(pShaderText);
    glShaderSource(ShaderObj, 1, p, Lengths);
    glCompileShader(ShaderObj);
    GLint success;
    glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar InfoLog[1024];
        glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
        fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
        exit(1);
    }
    glAttachShader(ShaderProgram, ShaderObj);
}

static void CompileShaders()
{
    GLuint ShaderProgram = glCreateProgram();
    if (ShaderProgram == 0) {
        fprintf(stderr, "Error creating shader program\n");
        exit(1);
    }
    string vs, fs;
    if (!ReadFile(pVSFileName, vs)) exit(1);
    if (!ReadFile(pFSFileName, fs)) exit(1);
    AddShader(ShaderProgram, vs.c_str(), GL_VERTEX_SHADER);
    AddShader(ShaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);
    GLint Success = 0;
    GLchar ErrorLog[1024] = {0};
    glLinkProgram(ShaderProgram);
    glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);
    if (Success == 0) {
        glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
        fprintf(stderr, "Error linking shader program: '%s'\n", ErrorLog);
        exit(1);
    }
    glValidateProgram(ShaderProgram);
    glGetProgramiv(ShaderProgram, GL_VALIDATE_STATUS, &Success);
    if (!Success) {
        glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
        fprintf(stderr, "Invalid shader program: '%s'\n", ErrorLog);
        exit(1);
    }
    glUseProgram(ShaderProgram);
    gWorldLocation = glGetUniformLocation(ShaderProgram, "gWorld");
    gColorLocation = glGetUniformLocation(ShaderProgram, "objectColor");
}




// GAME LOGIC

void initGameBoard() {
    int bs = BOARD_SIZE;
    for (int r = 0; r < bs; r++) {
        for (int c = 0; c < bs; c++) {
            // split based on board size
            if ((bs % 3 == 0 && (r < bs/3 || r > 2*bs/3 - 1) && (c < bs/3 || c > 2*bs/3 - 1)) ||
                (bs % 3 == 1 && (r < (bs-1)/3 || r > 2*(bs-1)/3) && (c < (bs-1)/3 || c > 2*(bs-1)/3)) ||
                (bs % 3 == 2 && (r < (bs-2)/3 + 1 || r > 2*(bs-2)/3) && (c < (bs-2)/3 + 1 || c > 2*(bs-2)/3)))
                board[r][c] = INVALID;
            else
                board[r][c] = MARBLE;
        }
    }
    board[BOARD_SIZE/2][BOARD_SIZE/2] = EMPTY;
    moveHistory.push(board);
}

bool isValidMove(int sr, int sc, int dr, int dc) {
    if (sr < 0 || sr >= BOARD_SIZE || sc < 0 || sc >= BOARD_SIZE ||
        dr < 0 || dr >= BOARD_SIZE || dc < 0 || dc >= BOARD_SIZE)
        return false;
    if (!((abs(dr - sr) == 2 && sc == dc) || (abs(dc - sc) == 2 && sr == dr)))
        return false;
    int mr = (sr + dr) / 2;
    int mc = (sc + dc) / 2;
    if (board[sr][sc] != MARBLE || board[dr][dc] != EMPTY)
        return false;
    if (board[mr][mc] != MARBLE)
        return false;
    return true;
}

void performMove(int sr, int sc, int dr, int dc) {
    if (!isValidMove(sr, sc, dr, dc))
        return;
    board[sr][sc] = EMPTY;
    int mr = (sr + dr) / 2;
    int mc = (sc + dc) / 2;
    board[mr][mc] = EMPTY;
    board[dr][dc] = MARBLE;
    moveHistory.push(board);
    while (!redoHistory.empty()) redoHistory.pop();
}

void undoMove() {
    if (moveHistory.size() <= 1 || undosRemaining <= 0 || gameOver) return;
    redoHistory.push(moveHistory.top());
    moveHistory.pop();
    board = moveHistory.top();
	undosRemaining--;
}

void redoMove() {
    if (redoHistory.empty() || gameOver) return;
    board = redoHistory.top();
    moveHistory.push(redoHistory.top());
    redoHistory.pop();
}

// Check win/lose conditions
void checkGameState() {
    
    int marbleCount = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c] == MARBLE)
                marbleCount++;
        }
    }
    
    if (marbleCount == 1 && (!CENTERWIN || board[BOARD_SIZE/2][BOARD_SIZE/2] == MARBLE)) {
        gameOver = true;
        gameWon = true;
		gameEndTime = (float)glfwGetTime();
        return;
    }
    
    bool validMoveFound = false;
    for (int r = 0; r < BOARD_SIZE && !validMoveFound; r++) {
        for (int c = 0; c < BOARD_SIZE && !validMoveFound; c++) {
            if (board[r][c] == MARBLE) {
                // check if valid move exists in any direction
                if (isValidMove(r, c, r - 2, c) ||
                    isValidMove(r, c, r + 2, c) ||
                    isValidMove(r, c, r, c - 2) ||
                    isValidMove(r, c, r, c + 2))
                {
                    validMoveFound = true;
                }
            }
        }
    }
    if (!validMoveFound) {
        gameOver = true;
        gameLost = true;
		gameEndTime = (float)glfwGetTime();
    }
}




// DRAWING BOARD

// Matrix4f_custom Implementation
class Matrix4f_custom {
public:
    float m[4][4];
    Matrix4f_custom() {
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }
    Matrix4f_custom operator*(const Matrix4f_custom &other) const {
        Matrix4f_custom result;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                result.m[i][j] = 0;
                for (int k = 0; k < 4; k++) {
                    result.m[i][j] += m[i][k] * other.m[k][j];
                }
            }
        }
        return result;
    }
    static Matrix4f_custom InitTranslation(float x, float y, float z) {
        Matrix4f_custom result;
        result.m[0][3] = x;
        result.m[1][3] = y;
        result.m[2][3] = z;
        return result;
    }
    static Matrix4f_custom InitScale(float x, float y, float z) {
        Matrix4f_custom result;
        result.m[0][0] = x;
        result.m[1][1] = y;
        result.m[2][2] = z;
        return result;
    }
};


void drawBoard() {
    float cellSize = 2.0f / BOARD_SIZE;
    float offset = -1.0f + cellSize / 2;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c] == INVALID)
                continue;

            Matrix4f_custom cellTransform = Matrix4f_custom::InitTranslation(offset + c * cellSize,
                offset + (BOARD_SIZE - 1 - r) * cellSize,
                0.0f)
                * Matrix4f_custom::InitScale(cellSize * 0.9f, cellSize * 0.9f, 1.0f);
            glUniformMatrix4fv(gWorldLocation, 1, GL_TRUE, &cellTransform.m[0][0]);

            // Highlight selected cell
            if (r == selectedRow && c == selectedCol && !gameOver)
                glUniform4f(gColorLocation, 1.0f, 1.0f, 0.6f, 0.9f);
            else
                glUniform4f(gColorLocation, 0.3f, 0.3f, 0.3f, 0.8f);

            glBindVertexArray(squareVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
 
            if (board[r][c] == MARBLE) {
                // Slightly smaller scale and push forward along z to show on top
                Matrix4f_custom marbleTransform = cellTransform 
                    * Matrix4f_custom::InitScale(0.85f, 0.85f, 1.0f)
                    * Matrix4f_custom::InitTranslation(0.0f, 0.0f, -0.01f);
                glUniformMatrix4fv(gWorldLocation, 1, GL_TRUE, &marbleTransform.m[0][0]);
                glUniform4f(gColorLocation, 0.1f, 0.6f, 1.0f, 1.0f);
                glBindVertexArray(circleVAO);
                glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS + 2);
                glBindVertexArray(0);
            }
        }
    }
}




// INPUT HANDLING

void processMouseClick(double xpos, double ypos, int action) {
    if (action != GLFW_PRESS)
        return;
    double ndcX = (xpos / WINDOW_WIDTH) * 2.0 - 1.0;
    double ndcY = 1.0 - (ypos / WINDOW_HEIGHT) * 2.0;
    float cellSize = 2.0f / BOARD_SIZE;
    int col = (int)((ndcX + 1.0) / cellSize);
    int row = BOARD_SIZE - 1 - (int)((ndcY + 1.0) / cellSize);
    if (selectedRow == -1 && selectedCol == -1) {
        if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE &&
            board[row][col] == MARBLE)
        {
            selectedRow = row;
            selectedCol = col;
        }
    } else {
        if (isValidMove(selectedRow, selectedCol, row, col))
            performMove(selectedRow, selectedCol, row, col);
        selectedRow = selectedCol = -1;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        processMouseClick(xpos, ypos, action);
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_Z:
                undoMove();
                break;
            case GLFW_KEY_Y:
                redoMove();
                break;
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
            default:
                break;
        }
    }
}




// IMGUI WINDOWS

void initImGui(GLFWwindow *window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void renderImGui() {
    ImGuiIO& io = ImGui::GetIO();
    
    // check game state each frame
    if (!gameOver) {
        checkGameState();
    }
    
    // top-left: time (with one decimal)
    ImGui::SetNextWindowSize(ImVec2(100, 75), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(75, 75), ImGuiCond_Always);
    ImGui::Begin("Time", NULL, ImGuiWindowFlags_NoTitleBar |
                               ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoMove |
                               ImGuiWindowFlags_NoScrollbar |
                               ImGuiWindowFlags_NoBackground);
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextWrapped("Time: %.1f", !gameOver ? (float)glfwGetTime() : gameEndTime);
    ImGui::End();
    
    // top-right: marble count
    ImVec2 displaySize = io.DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(140, 75), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(displaySize.x - 175, 75), ImGuiCond_Always);
    ImGui::Begin("Marbles", NULL, ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoBackground);
    ImGui::SetWindowFontScale(2.0f);
    int marbleCount = 0;
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            if (board[r][c] == MARBLE)
                marbleCount++;
    ImGui::TextWrapped("Marbles: %d", marbleCount);
    ImGui::End();
    
    // bottom-left: instructions for moves
    ImGui::SetNextWindowSize(ImVec2(175, 50), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(100, 100), ImVec2(300, 500));
    ImGui::SetNextWindowPos(ImVec2(30, displaySize.y - 150), ImGuiCond_Always);
    ImGui::Begin("Instructions", NULL, ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoScrollbar |
                                        ImGuiWindowFlags_NoBackground);
    ImGui::TextWrapped("Click on a marble to select, then click on a valid destination.");
    ImGui::End();
    
    // bottom-right: undo/redo instructions
    ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(100, 100), ImVec2(300, 500));
    ImGui::SetNextWindowPos(ImVec2(displaySize.x - 180, displaySize.y - 150), ImGuiCond_Always);
    ImGui::Begin("Undo_Redo", NULL, ImGuiWindowFlags_NoTitleBar |
                                         ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_AlwaysAutoResize |
                                         ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoScrollbar |
                                         ImGuiWindowFlags_NoBackground);
    ImGui::TextWrapped("Press 'Z' to undo, 'Y' to redo.");
    ImGui::TextWrapped("You have %d undos remaining.", undosRemaining);
    ImGui::End();

    // win/lose popup
    if (gameOver) {
		ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(250, 275), ImGuiCond_Always);
        const char* popupTitle = gameWon ? "You Win!" : "Game Over";
        if (ImGui::BeginPopupModal(popupTitle, NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
			ImGui::SetWindowFontScale(2.0f);
			if (gameWon) ImGui::TextWrapped("Congratulations! You win!\nTime taken: %.1f seconds", gameEndTime);
            else ImGui::TextWrapped("No valid moves remain. You lose. dumbass\nMarbles remaining: %d", marbleCount);

            ImGui::Spacing();
            if (ImGui::Button("Replay", ImVec2(285, 30))) {
                ImGui::CloseCurrentPopup();
                // reset game state
                initGameBoard();
                gameOver = false;
                gameWon = false;
                gameLost = false;
                undosRemaining = 3;
				while (!redoHistory.empty()) redoHistory.pop();
				while (!moveHistory.empty()) moveHistory.pop();
				glfwSetTime(0);
            }
            ImGui::EndPopup();
        }
        else {
            ImGui::OpenPopup(popupTitle);
        }
    }
}




// MAIN LOOP

int main(int argc, char *argv[]) {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, programTitle, NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    glewInit();
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);
    
    CreateSquareVertexBuffer();
    CreateCircleVertexBuffer();
    CompileShaders();
    glEnable(GL_DEPTH_TEST);
    
    initGameBoard();
    initImGui(window);
    
    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        drawBoard();
        renderImGui();
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
