# CG Assignment 1 - Marble Solitaire (Brainvita)

**Roll No:** 2023101090 \
**Name:** Anurag Peddi



## Instructions to Run

1. Install glut/freeglut and GLEW by using the following command in the terminal:
   ```
   sudo apt-get install freeglut3-dev libglew-dev
   ```
2. Verifying installation. Follow the following steps:
   ```
   sudo apt-get install mesa-utils
   glxinfo | grep "direct rendering" ### Expected output -> direct rendering: Yes
   glxinfo | grep "OpenGL core profile version" ### Expected output -> Some version number
   glxgears ### Should run an animation having 3 gears. Verifies that setup is OK.
   ```

4. Run `make` to build the project.
5. Run `./marble` to start the game.

## How ImGui Is Used
- **User Interface:**
ImGui is used to display game information such as the current time, number of marbles left, and instructions for undo/redo.

- **Dynamic Layout:**
The UI windows are relative to the window so that they remain in the correct places even when the window is resized.

- **Popup Modal:**
When the game ends (win or lose), an ImGui popup appears in the center with a message and a button to replay the game.

## Observations and Effort
- **Rendering:**
Making the marbles appear above the cells took a bit of effort since they were always getting rendered behind them. Rendering everything else was pretty much straightforward.

- **Input detection**
Figuring out which cell was clicked wasn't too difficult, but it took some time to think and figure out what the formula was.

- **Game Logic:**
The game logic part was easy since I've done similar projects before. The win/lose handling was a bit comlpicated, though (since the UI also depended on it).

- **ImGui Integration:**
ImGui was simple to integrate and greatly helped in displaying dynamic game information without complex UI coding. Getting a satisfactory format for all of the windows was a bit frustrating, though.
