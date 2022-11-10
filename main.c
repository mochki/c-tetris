/***************************************************************************************
 * TETRIS
 * 		Author : mochki
 *
 * 		Childhood classic game of tetris to run on MSP430G2553 & with a joystick, 3
 * 		buttons and adafruit ILI9341 touchscreen 320x240 display.
 **************************************************************************************/
#include "msp430g2553.h"
#include <stdbool.h>

// Pin Definitions
#define TS_XM			0x0001		// P1.0 : X-
#define TS_YP			0x0002		// P1.1 : Y+
#define TS_XP			0x0004		// P1.2 : X+
#define TS_YM			0x0008		// P1.3 : Y-
														// P1.4 : J1 (Y-axis Joystick)
#define BTN_ROT		0x0040		// P1.6 : rotate btn
#define BTN_RGHT	0x0010		// P2.3 : right btn
#define BTN_LFT		0x0008		// P2.4 : left btn
														// Interrupts written backwards, this works
#define LCD_SCK		0x0020		// P1.5 : serial clk
#define LCD_MOSI	0x0080		// P1.7 : data out
#define LCD_RST 	0x0001		// P2.0 : reset
#define LCD_CS		0x0002		// P2.1 : chip select
#define LCD_DC		0x0004		// P2.2 : Data/Cmd

// Function Prototypes
void writeLCDData(char);
void writeLCDControl(char);
void waitMS(unsigned int);
void initClk(void);
void initPins(void);
void initLCD(void);
void initUSCI(void);
void initBackground(void);
void readTS(void);
void drawPixel(int, int, int);
void drawLetter(char, char, char, int);
void drawInstruction(char, char, int);
void fillScreen(int);
void checkCollisions(void);
void placePiece(void);
void removePiece(void);
void drawGrid(void);
void drawSquare(int, int, int);
void drawPiece(int, int);
void erasePiece(int, int);
void setLevelColor(void);
void drawLevelColor(void);
void drawScore(void);
void checkGameOver(void);
void step(void);

// Global Variables												// most are self descriptive
unsigned int z;														// touchscreen touch pressure
unsigned int level = 1;
unsigned int linesCleared = 0;
unsigned int scoreVerticlePosition = 0;		// scoring system on top right
unsigned int piece;												// what piece is in play
unsigned int rotation;										// rotation of piece
unsigned int xPos;												// top left corner of piece
unsigned int yPos;
unsigned int dropCounter = 0;							// timer to slow/speed up block fall
unsigned int keyPress = 1;								// needed for 'random' algorithm
unsigned int levelColor = 0xAEBB;					// cycles as level up
unsigned int scoreColumn = 0;							// scoring variables
unsigned int scoreRow = 0;
unsigned int downJoystick;
unsigned int graceTime = 0;								// block touches but it's still 'alive'
bool start = false;
bool pieceAlive = false;
bool leftKey = false;
bool rightKey = false;
bool rotateKey = false;
bool fullLine = false;
bool canRotate = true;
bool canRight = true;
bool canLeft = true;
bool canDown = true;
bool gameAlive = true;

unsigned int grid[14][10] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/***************************************************************************************
 * MAIN
 * 		Initialize game variables and setup. Send control to STEP for main game logic
 **************************************************************************************/
void main(void) {
	WDTCTL = WDTPW + WDTHOLD;		// Stop watchdog timer

	initClk();									// Init clock to 20 MHz
	initPins();									// Init Pin Functionality
	initUSCI();									// Init USCI (SPI)
	initLCD();									// Init LCD Controller
	fillScreen(0x5B57);					// Init start screen w/ instruction
	drawInstruction(85, 61, 0x0000);
	drawInstruction(84, 60, 0xFFFF);

	// Don't start until a touchscreen input
	while (!start) {
		readTS();
		if (z > 100) {
			initBackground();				// main UI draw
			drawLevelColor();
			start = true;
		}
	}

	start = false;							// for when we start again

	while (1) {
		step();										// always stay in this loop
	}
}

/***************************************************************************************
 * STEP
 * 		All game movement handling per cycle
 **************************************************************************************/
void step() {
	int i;					// for loops
	int j;
	int k;

	while (gameAlive) {
		checkCollisions();				// get status of where piece can move

		if (!canDown) {
			graceTime++;						// increment so piece stays alive

			if (graceTime > 9000) {
				pieceAlive = false;		// okay, piece is set

				// loop through rows to see if one is full
				for (i = 0; i < 14; i++) {
					fullLine = true;
					for (j = 0; j < 10; j++)
						if (grid[i][j] == 0)
							fullLine = false;

					// if so, delete line
					if (fullLine) {
						for (k = 0; (i - k) >= 0; k++)
							for (j = 0; j < 10; j++)
								if (i - k == 0)				// fill with zero if top row
									grid[i - k][j] = 0;
								else									// else fill with above row
									grid[i - k][j] = grid[i - (1 + k)][j];

						drawScore();								// score columns formatting and outputting
						linesCleared++;
						scoreRow += 2;
						if (linesCleared % 10 == 0) {
							scoreColumn += 2;
							scoreRow = 0;
						}

						// level up logic, color
						if (linesCleared == level * 10) {
							level++;
							linesCleared = 0;
							setLevelColor();
							drawLevelColor();
						}
						drawGrid();									// and draw the entire board
					}
				}
				graceTime = 0;
			}
		}

		// piece move/draw logic
		if (!pieceAlive) {
			// new piece, start loc, random piece gen, check gameover
			xPos = 4;
			yPos = 0;
			piece = (keyPress % 7) + 1;
			rotation = 0;
			checkGameOver();
			placePiece();
			pieceAlive = true;
			drawPiece(20+(20*xPos), 30+(20*yPos));
		} else if (leftKey && canLeft) {						// simple left shift
			erasePiece(20+(20*xPos), 30+(20*yPos));
			removePiece();
			xPos--;
			placePiece();
			drawPiece(20+(20*xPos), 30+(20*yPos));
			leftKey = false;
		} else if (rightKey && canRight) {					// simple right shift
			erasePiece(20+(20*xPos), 30+(20*yPos));
			removePiece();
			xPos++;
			placePiece();
			drawPiece(20+(20*xPos), 30+(20*yPos));
			rightKey = false;
		} else if (rotateKey && canRotate) {				// rotate logic and erase/draw
			erasePiece(20+(20*xPos), 30+(20*yPos));
			removePiece();
			if (rotation <3)
				rotation++;
			else
				rotation = 0;
			placePiece();
			drawPiece(20+(20*xPos), 30+(20*yPos));
			rotateKey = false;
		}

		// with arbitrary counter, piece drops at according rate
		if (dropCounter > (24000 - (level-1)*1166) && canDown) {
			erasePiece(20+(20*xPos), 30+(20*yPos));
			removePiece();
			yPos++;
			placePiece();
			drawPiece(20+(20*xPos), 30+(20*yPos));
			leftKey = false;
			rightKey = false;
			rotateKey = false;
			keyPress++;
			dropCounter = 0;
		}

		// joystick y-axis read
		ADC10CTL0 = ADC10SHT_2 + ADC10ON;
		ADC10CTL1 = INCH_4;
		ADC10AE0 = BIT4;
		ADC10CTL0 |= ENC + ADC10SC;
		while (ADC10CTL1 & 0x0001);
		downJoystick = ADC10MEM;

		// different amounts of pull, pulls piece faster
		if (downJoystick < 100)
			dropCounter += 24000;
		else if (downJoystick < 200)
			dropCounter += 500;
		else if (downJoystick < 455)
			dropCounter += 100;

		// each cycle increments to give drop rate
		dropCounter++;
	}

	// if we get a game over we come here
	// "tap to start"
	drawInstruction(85, 61, 0x0000);
	drawInstruction(84, 60, 0xFFFF);

	// reinitialize everything after waiting for touchscreen
	while (!start) {
		readTS();
		if (z > 100) {
			level = 1;
			linesCleared = 0;
			scoreVerticlePosition = 0;
			dropCounter = 0;
			keyPress = 1;
			levelColor = 0xAEBB;
			scoreColumn = 0;
			scoreRow = 0;
			pieceAlive = false;
			leftKey = false;
			rightKey = false;
			rotateKey = false;
			fullLine = false;
			canRotate = true;
			canRight = true;
			canLeft = true;
			canDown = true;
			gameAlive = true;
			initBackground();
			drawLevelColor();
			start = true;

			// fills grid to full empty 0's
			for (i = 0; i < 14; i++) {
				for (j = 0; j < 10; j++) {
					grid[i][j] = 0;
				}
			}
		}
	}
	start = false;
}

/***************************************************************************************
 * CHECK COLLISIONS
 * 		Self exlpanatory, but checks alive piece's ability to go left, right, down, rotate
 * 		High asserted, everything is true until set false.
 **************************************************************************************/
void checkCollisions(void) {
	canLeft = true;
	canRight = true;
	canRotate = true;
	canDown = true;

	if (xPos == 0)
		canLeft = false;							// Regardless of piece, cant go left from here

	// takes piece then rotation -> checks left, right, rotate down
	switch (piece) {
	case 1:	// O
		if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos - 1] != 0)
				canLeft = false;
		if (grid[yPos][xPos + 2] != 0 || grid[yPos + 1][xPos + 2] != 0 || xPos == 8)
				canRight = false;
		if (yPos == 12 || grid[yPos + 2][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0)
			canDown = false;
		break;
	case 2: // I
		switch (rotation) {
		case 0:
		case 2:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos - 1] != 0 || grid[yPos + 2][xPos - 1] != 0 || grid[yPos + 3][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 1] != 0 || grid[yPos + 1][xPos + 1] != 0 || grid[yPos + 2][xPos + 1] != 0 || grid[yPos + 3][xPos + 1] != 0 || xPos == 9)
				canRight = false;
			if (xPos > 6 || grid[yPos][xPos + 1] != 0 || grid[yPos][xPos + 2] != 0 || grid[yPos][xPos + 3] != 0)
				canRotate = false;
			if (yPos == 10 || grid[yPos + 4][xPos] != 0)
				canDown = false;
			break;
		case 1:
		case 3:
			if (grid[yPos][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 4] != 0 || xPos == 6)
				canRight = false;
			if (xPos == 9 || grid[yPos + 1][xPos] != 0 || grid[yPos + 2][xPos] != 0 || grid[yPos + 3][xPos] != 0)
				canRotate = false;
			if (yPos == 13 || grid[yPos + 1][xPos] != 0 || grid[yPos + 1][xPos + 1] != 0 || grid[yPos + 1][xPos + 2] != 0 || grid[yPos + 1][xPos + 3] != 0)
				canDown = false;
			break;
		}
		break;
	case 3: // Z
		switch (rotation) {
		case 0:
		case 2:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 2] != 0 || grid[yPos + 1][xPos + 3] != 0 || xPos == 7)
				canRight = false;
			if (grid[yPos + 1][xPos] != 0 || grid[yPos + 2][xPos] != 0)
				canRotate = false;
			if (yPos == 12 || grid[yPos + 1][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0 || grid[yPos + 2][xPos + 2] != 0)
				canDown = false;
			break;
		case 1:
		case 3:
			if (grid[yPos + 1][xPos - 1] != 0 || grid[yPos + 2][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 2] != 0 || grid[yPos + 1][xPos + 2] != 0 || xPos == 8)
				canRight = false;
			if (xPos == 9 || grid[yPos][xPos] != 0 || grid[yPos + 1][xPos + 2] != 0)
				canRotate = false;
			if (yPos == 11 || grid[yPos + 3][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0)
				canDown = false;
			break;
		}
		break;
	case 4: // S
		switch (rotation) {
		case 0:
		case 2:
			if (grid[yPos][xPos] != 0 || grid[yPos + 1][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 3] != 0 || grid[yPos + 1][xPos + 2] != 0 || xPos == 7)
				canRight = false;
			if (grid[yPos][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0)
				canRotate = false;
			if (yPos == 12 || grid[yPos + 2][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0 || grid[yPos + 1][xPos + 2] != 0)
				canDown = false;
			break;
		case 1:
		case 3:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos - 1] != 0 || grid[yPos + 2][xPos] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 1] != 0 || grid[yPos + 1][xPos + 2] != 0 || grid[yPos + 2][xPos + 2] != 0 || xPos == 8)
				canRight = false;
			if (xPos == 9 || grid[yPos][xPos + 1] != 0 || grid[yPos][xPos + 2] != 0)
				canRotate = false;
			if (yPos == 11 || grid[yPos + 2][xPos] != 0 || grid[yPos + 3][xPos + 1] != 0)
				canDown = false;
			break;
		}
		break;
	case 5: // J
		switch (rotation) {
		case 0:
			if (grid[yPos][xPos] != 0 || grid[yPos + 1][xPos] != 0 || grid[yPos + 2][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 2] != 0 || grid[yPos + 1][xPos + 2] != 0 || grid[yPos + 2][xPos + 2] != 0 || xPos == 8)
				canRight = false;
			if (xPos > 7 || grid[yPos][xPos] != 0 || grid[yPos + 1][xPos] != 0 || grid[yPos + 1][xPos + 2] != 0)
				canRotate = false;
			if (yPos == 11 || grid[yPos + 3][xPos] != 0 || grid[yPos + 3][xPos + 1] != 0)
				canDown = false;
			break;
		case 1:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 1] != 0 || grid[yPos + 1][xPos + 3] != 0 || xPos == 7)
				canRight = false;
			if (grid[yPos][xPos + 1] != 0 || grid[yPos + 2][xPos] != 0)
				canRotate = false;
			if (yPos == 12 || grid[yPos + 2][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0 || grid[yPos + 2][xPos + 2] != 0)
				canDown = false;
			break;
		case 2:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos - 1] != 0 || grid[yPos + 2][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 2] != 0 || grid[yPos + 1][xPos + 1] != 0 || grid[yPos + 2][xPos + 1] != 0 || xPos == 8)
				canRight = false;
			if (xPos > 7 || grid[yPos + 1][xPos + 2] != 0 || grid[yPos][xPos + 2] != 0)
				canRotate = false;
			if (yPos == 11 || grid[yPos + 3][xPos] != 0 || grid[yPos + 1][xPos + 1] != 0)
				canDown = false;
			break;
		case 3:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos + 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 3] != 0 || grid[yPos + 1][xPos + 3] != 0 || xPos == 7)
				canRight = false;
			if (grid[yPos + 1][xPos + 1] != 0 || grid[yPos + 2][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0)
				canRotate = false;
			if (yPos == 12 || grid[yPos + 1][xPos] != 0 || grid[yPos + 1][xPos + 1] != 0 || grid[yPos + 2][xPos + 2] != 0)
				canDown = false;
			break;
		}
		break;
	case 6: // L
		switch (rotation) {
		case 0:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos - 1] != 0 || grid[yPos + 2][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 1] != 0 || grid[yPos + 1][xPos + 1] != 0 || grid[yPos + 2][xPos + 2] != 0 || xPos == 8)
				canRight = false;
			if (xPos > 7 || grid[yPos][xPos + 1] != 0 || grid[yPos][xPos + 2] != 0)
				canRotate = false;
			if (yPos == 11 || grid[yPos + 3][xPos] != 0 || grid[yPos + 3][xPos + 1] != 0)
				canDown = false;
			break;
		case 1:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 3] != 0 || grid[yPos + 1][xPos + 1] != 0 || xPos == 7)
				canRight = false;
			if (grid[yPos + 1][xPos + 1] != 0 || grid[yPos + 2][xPos + 1] != 0)
				canRotate = false;
			if (yPos == 12 || grid[yPos + 2][xPos] != 0 || grid[yPos + 1][xPos + 1] != 0 || grid[yPos + 1][xPos + 2] != 0)
				canDown = false;
			break;
		case 2:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos] != 0 || grid[yPos + 2][xPos] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 2] != 0 || grid[yPos + 1][xPos + 2] != 0 || grid[yPos + 2][xPos + 2] != 0 || xPos == 8)
				canRight = false;
			if (xPos > 7 || grid[yPos][xPos + 2] != 0 || grid[yPos + 1][xPos] != 0 || grid[yPos + 1][xPos + 2] != 0)
				canRotate = false;
			if (yPos == 11 || grid[yPos + 1][xPos] != 0 || grid[yPos + 3][xPos + 1] != 0)
				canDown = false;
			break;
		case 3:
			if (grid[yPos][xPos + 1] != 0 || grid[yPos + 1][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 3] != 0 || grid[yPos + 1][xPos + 3] != 0 || xPos == 7)
				canRight = false;
			if (grid[yPos][xPos] != 0 || grid[yPos + 2][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0)
				canRotate = false;
			if (yPos == 12 || grid[yPos + 2][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0 || grid[yPos + 2][xPos + 2] != 0)
				canDown = false;
			break;
		}
		break;
	case 7: // T
		switch (rotation) {
		case 0:
			if (grid[yPos][xPos] != 0 || grid[yPos + 1][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 2] != 0 || grid[yPos + 1][xPos + 3] != 0 || xPos == 7)
				canRight = false;
			if (grid[yPos][xPos] != 0 || grid[yPos + 2][xPos] != 0)
				canRotate = false;
			if (yPos == 12 || grid[yPos + 2][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0 || grid[yPos + 2][xPos + 2] != 0)
				canDown = false;
			break;
		case 1:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos - 1] != 0 || grid[yPos + 2][xPos - 1] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 1] != 0 || grid[yPos + 1][xPos + 2] != 0 || grid[yPos + 2][xPos + 1] != 0 || xPos == 8)
				canRight = false;
			if (xPos > 7 || grid[yPos][xPos + 1] != 0 || grid[yPos][xPos + 2] != 0)
				canRotate = false;
			if (yPos == 11 || grid[yPos + 3][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0)
				canDown = false;
			break;
		case 2:
			if (grid[yPos][xPos - 1] != 0 || grid[yPos + 1][xPos] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 3] != 0 || grid[yPos + 1][xPos + 2] != 0 || xPos == 7)
				canRight = false;
			if (grid[yPos + 1][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0)
				canRotate = false;
			if (yPos == 12 || grid[yPos + 1][xPos] != 0 || grid[yPos + 2][xPos + 1] != 0 || grid[yPos + 1][xPos + 2] != 0)
				canDown = false;
			break;
		case 3:
			if (grid[yPos][xPos] != 0 || grid[yPos + 1][xPos - 1] != 0 || grid[yPos + 2][xPos] != 0)
				canLeft = false;
			if (grid[yPos][xPos + 2] != 0 || grid[yPos + 1][xPos + 2] != 0 || grid[yPos + 2][xPos + 2] != 0 || xPos == 8)
				canRight = false;
			if (xPos > 7 || grid[yPos + 1][xPos + 2] != 0)
				canRotate = false;
			if (yPos == 11 || grid[yPos + 2][xPos] != 0 || grid[yPos + 3][xPos + 1] != 0)
				canDown = false;
			break;
		}
		break;
	}
}

/***************************************************************************************
 * CHECK GAME OVER
 * 		Checks to see if there is already blocks in the place the new piece is generated.
 * 		If so, the game is over.
 **************************************************************************************/
void checkGameOver(void) {
	switch (piece) {
	case 1:
		if (grid[yPos][xPos] != 0 | grid[yPos + 1][xPos] != 0 | grid[yPos][xPos + 1] != 0 | grid[yPos + 1][xPos + 1] != 0)
			gameAlive = false;
		break;
	case 2:
		if (grid[yPos][xPos] != 0 | grid[yPos + 1][xPos] != 0 | grid[yPos + 2][xPos] != 0 | grid[yPos + 3][xPos] != 0)
			gameAlive = false;
		break;
	case 3:
		if (grid[yPos][xPos] != 0 | grid[yPos][xPos + 1] != 0 | grid[yPos + 1][xPos + 1] != 0 | grid[yPos + 1][xPos + 2] != 0)
			gameAlive = false;
		break;
	case 4:
		if (grid[yPos][xPos + 1] != 0 | grid[yPos][xPos + 2] != 0 | grid[yPos + 1][xPos] != 0 | grid[yPos + 1][xPos + 1] != 0)
			gameAlive = false;
		break;
	case 5:
		if (grid[yPos][xPos + 1] != 0 | grid[yPos + 1][xPos + 1] != 0 | grid[yPos + 2][xPos] != 0 | grid[yPos + 2][xPos + 1] != 0)
			gameAlive = false;
		break;
	case 6:
		if (grid[yPos][xPos] != 0 | grid[yPos + 1][xPos] != 0 | grid[yPos + 2][xPos] != 0 | grid[yPos + 2][xPos + 1] != 0)
			gameAlive = false;
		break;
	case 7:
		if (grid[yPos][xPos + 1] != 0 | grid[yPos + 1][xPos] != 0 | grid[yPos +1][xPos + 1] != 0 | grid[yPos + 1][xPos + 2] != 0)
			gameAlive = false;
		break;
	}
}

/***************************************************************************************
 * PLACE PIECE
 * 		Based on the piece and the rotation, this places the piece on the grid array,
 * 		starting with the top left location
 **************************************************************************************/
void placePiece(void) {
	switch (piece) {
	case 1: // O
		grid[yPos][xPos] = piece;
		grid[yPos + 1][xPos] = piece;
		grid[yPos][xPos + 1] = piece;
		grid[yPos + 1][xPos + 1] = piece;
		break;
	case 2: // I
		switch (rotation) {
		case 0:
		case 2:
			grid[yPos][xPos] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 2][xPos] = piece;
			grid[yPos + 3][xPos] = piece;
			break;
		case 1:
		case 3:
			grid[yPos][xPos] = piece;
			grid[yPos][xPos + 1] = piece;
			grid[yPos][xPos + 2] = piece;
			grid[yPos][xPos + 3] = piece;
			break;
		}
		break;
	case 3: // Z
		switch (rotation) {
		case 0:
		case 2:
			grid[yPos][xPos] = piece;
			grid[yPos][xPos + 1] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			grid[yPos + 1][xPos + 2] = piece;
			break;
		case 1:
		case 3:
			grid[yPos][xPos + 1] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			grid[yPos + 2][xPos] = piece;
			break;
		}
		break;
	case 4: // S
		switch (rotation) {
		case 0:
		case 2:
			grid[yPos][xPos + 1] = piece;
			grid[yPos][xPos + 2] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			break;
		case 1:
		case 3:
			grid[yPos][xPos] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			grid[yPos + 2][xPos + 1] = piece;
			break;
		}
		break;
	case 5: // J
		switch (rotation) {
		case 0:
			grid[yPos][xPos + 1] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			grid[yPos + 2][xPos] = piece;
			grid[yPos + 2][xPos + 1] = piece;
			break;
		case 1:
			grid[yPos][xPos] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			grid[yPos + 1][xPos + 2] = piece;
			break;
		case 2:
			grid[yPos][xPos] = piece;
			grid[yPos][xPos + 1] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 2][xPos] = piece;
			break;
		case 3:
			grid[yPos][xPos] = piece;
			grid[yPos][xPos + 1] = piece;
			grid[yPos][xPos + 2] = piece;
			grid[yPos + 1][xPos + 2] = piece;
			break;
		}
		break;
	case 6: // L
		switch (rotation) {
		case 0:
			grid[yPos][xPos] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 2][xPos] = piece;
			grid[yPos + 2][xPos + 1] = piece;
			break;
		case 1:
			grid[yPos][xPos] = piece;
			grid[yPos][xPos + 1] = piece;
			grid[yPos][xPos + 2] = piece;
			grid[yPos + 1][xPos] = piece;
			break;
		case 2:
			grid[yPos][xPos] = piece;
			grid[yPos][xPos + 1] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			grid[yPos + 2][xPos + 1] = piece;
			break;
		case 3:
			grid[yPos][xPos + 2] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			grid[yPos + 1][xPos + 2] = piece;
			break;
		}
		break;
	case 7: // T
		switch (rotation) {
		case 0:
			grid[yPos][xPos + 1] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			grid[yPos + 1][xPos + 2] = piece;
			break;
		case 1:
			grid[yPos][xPos] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			grid[yPos + 2][xPos] = piece;
			break;
		case 2:
			grid[yPos][xPos] = piece;
			grid[yPos][xPos + 1] = piece;
			grid[yPos][xPos + 2] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			break;
		case 3:
			grid[yPos][xPos + 1] = piece;
			grid[yPos + 1][xPos] = piece;
			grid[yPos + 1][xPos + 1] = piece;
			grid[yPos + 2][xPos + 1] = piece;
			break;
		}
		break;
	}
}

/***************************************************************************************
 * REMOVE PIECE
 * 		Based on the piece and the rotation, this removes the piece from the grid array,
 * 		starting with the top left location
 **************************************************************************************/
void removePiece(void) {
	switch (piece) {
	case 1: // O
		grid[yPos][xPos] = 0;
		grid[yPos + 1][xPos] = 0;
		grid[yPos][xPos + 1] = 0;
		grid[yPos + 1][xPos + 1] = 0;
		break;
	case 2: // I
		switch (rotation) {
		case 0:
		case 2:
			grid[yPos][xPos] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 2][xPos] = 0;
			grid[yPos + 3][xPos] = 0;
			break;
		case 1:
		case 3:
			grid[yPos][xPos] = 0;
			grid[yPos][xPos + 1] = 0;
			grid[yPos][xPos + 2] = 0;
			grid[yPos][xPos + 3] = 0;
			break;
		}
		break;
	case 3: // Z
		switch (rotation) {
		case 0:
		case 2:
			grid[yPos][xPos] = 0;
			grid[yPos][xPos + 1] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			grid[yPos + 1][xPos + 2] = 0;
			break;
		case 1:
		case 3:
			grid[yPos][xPos + 1] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			grid[yPos + 2][xPos] = 0;
			break;
		}
		break;
	case 4: // S
		switch (rotation) {
		case 0:
		case 2:
			grid[yPos][xPos + 1] = 0;
			grid[yPos][xPos + 2] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			break;
		case 1:
		case 3:
			grid[yPos][xPos] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			grid[yPos + 2][xPos + 1] = 0;
			break;
		}
		break;
	case 5: // J
		switch (rotation) {
		case 0:
			grid[yPos][xPos + 1] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			grid[yPos + 2][xPos] = 0;
			grid[yPos + 2][xPos + 1] = 0;
			break;
		case 1:
			grid[yPos][xPos] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			grid[yPos + 1][xPos + 2] = 0;
			break;
		case 2:
			grid[yPos][xPos] = 0;
			grid[yPos][xPos + 1] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 2][xPos] = 0;
			break;
		case 3:
			grid[yPos][xPos] = 0;
			grid[yPos][xPos + 1] = 0;
			grid[yPos][xPos + 2] = 0;
			grid[yPos + 1][xPos + 2] = 0;
			break;
		}
		break;
	case 6: // L
		switch (rotation) {
		case 0:
			grid[yPos][xPos] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 2][xPos] = 0;
			grid[yPos + 2][xPos + 1] = 0;
			break;
		case 1:
			grid[yPos][xPos] = 0;
			grid[yPos][xPos + 1] = 0;
			grid[yPos][xPos + 2] = 0;
			grid[yPos + 1][xPos] = 0;
			break;
		case 2:
			grid[yPos][xPos] = 0;
			grid[yPos][xPos + 1] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			grid[yPos + 2][xPos + 1] = 0;
			break;
		case 3:
			grid[yPos][xPos + 2] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			grid[yPos + 1][xPos + 2] = 0;
			break;
		}
		break;
	case 7: // T
		switch (rotation) {
		case 0:
			grid[yPos][xPos + 1] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			grid[yPos + 1][xPos + 2] = 0;
			break;
		case 1:
			grid[yPos][xPos] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			grid[yPos + 2][xPos] = 0;
			break;
		case 2:
			grid[yPos][xPos] = 0;
			grid[yPos][xPos + 1] = 0;
			grid[yPos][xPos + 2] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			break;
		case 3:
			grid[yPos][xPos + 1] = 0;
			grid[yPos + 1][xPos] = 0;
			grid[yPos + 1][xPos + 1] = 0;
			grid[yPos + 2][xPos + 1] = 0;
			break;
		}
		break;
	}
}

/***************************************************************************************
 * DRAW SQUARE
 * 		Based on the piece, this draws the outer then inner square starting at the top
 * 		left x, y position. Different pieces have different colors.
 **************************************************************************************/
void drawSquare(int x0, int y, int pieceNumber) {
	int i;
	// because we have to split up the location bytes for y
	int y0 = y / 256;
	int y1 = y % 256;
	int y2 = (y+19) / 256;
	int y3 = (y+19) % 256;
	int y4 = (y+2) / 256;
	int y5 = (y+2) % 256;
	int y6 = (y+17) / 256;
	int y7 = (y+17) % 256;

	// outer square
	writeLCDControl(0x2A);		// Select Column Address
	writeLCDData(0);					// Setup beginning column address
	writeLCDData(x0);					// Setup beginning column address
	writeLCDData(0);					// Setup ending column address
	writeLCDData(x0+19);			// Setup ending column address
	writeLCDControl(0x2B);		// Select Row Address
	writeLCDData(y0);					// Setup beginning row address
	writeLCDData(y1);					// Setup beginning row address
	writeLCDData(y2);					// Setup ending row address
	writeLCDData(y3);					// Setup ending row address
	writeLCDControl(0x2C);		// Select Memory Write
	switch (pieceNumber) {
	case 0:
		for (i = 220; i > 0; i--) {			// Loop through all memory locations 16 bit color
			writeLCDData(0x110F >> 8);		// Write data to LCD memory
			writeLCDData(0x110F & 0xff);	// Write data to LCD memory
			writeLCDData(0x110F >> 8);		// Write data to LCD memory
			writeLCDData(0x110F & 0xff);	// Write data to LCD memory
		}
		break;
	case 1:
		for (i = 220; i > 0; i--) {			// Loop through all memory locations 16 bit color
			writeLCDData(0x69D6 >> 8);		// Write data to LCD memory
			writeLCDData(0x69D6 & 0xff);	// Write data to LCD memory
			writeLCDData(0x69D6 >> 8);		// Write data to LCD memory
			writeLCDData(0x69D6 & 0xff);	// Write data to LCD memory
		}
		break;
	case 2:
		for (i = 220; i > 0; i--) {			// Loop through all memory locations 16 bit color
			writeLCDData(0x24BD >> 8);		// Write data to LCD memory
			writeLCDData(0x24BD & 0xff);	// Write data to LCD memory
			writeLCDData(0x24BD >> 8);		// Write data to LCD memory
			writeLCDData(0x24BD & 0xff);	// Write data to LCD memory
		}
		break;
	case 3:
		for (i = 220; i > 0; i--) {			// Loop through all memory locations 16 bit color
			writeLCDData(0x053D >> 8);		// Write data to LCD memory
			writeLCDData(0x053D & 0xff);	// Write data to LCD memory
			writeLCDData(0x053D >> 8);		// Write data to LCD memory
			writeLCDData(0x053D & 0xff);	// Write data to LCD memory
		}
		break;
	case 4:
		for (i = 220; i > 0; i--) {			// Loop through all memory locations 16 bit color
			writeLCDData(0x05D9 >> 8);		// Write data to LCD memory
			writeLCDData(0x05D9 & 0xff);	// Write data to LCD memory
			writeLCDData(0x05D9 >> 8);		// Write data to LCD memory
			writeLCDData(0x05D9 & 0xff);	// Write data to LCD memory
		}
		break;
	case 5:
		for (i = 220; i > 0; i--) {			// Loop through all memory locations 16 bit color
			writeLCDData(0x3A96 >> 8);		// Write data to LCD memory
			writeLCDData(0x3A96 & 0xff);	// Write data to LCD memory
			writeLCDData(0x3A96 >> 8);		// Write data to LCD memory
			writeLCDData(0x3A96 & 0xff);	// Write data to LCD memory
		}
		break;
	case 6:
		for (i = 220; i > 0; i--) {			// Loop through all memory locations 16 bit color
			writeLCDData(0x9135 >> 8);		// Write data to LCD memory
			writeLCDData(0x9135 & 0xff);	// Write data to LCD memory
			writeLCDData(0x9135 >> 8);		// Write data to LCD memory
			writeLCDData(0x9135 & 0xff);	// Write data to LCD memory
		}
		break;
	case 7:
		for (i = 220; i > 0; i--) {			// Loop through all memory locations 16 bit color
			writeLCDData(0x03D2 >> 8);		// Write data to LCD memory
			writeLCDData(0x03D2 & 0xff);	// Write data to LCD memory
			writeLCDData(0x03D2 >> 8);		// Write data to LCD memory
			writeLCDData(0x03D2 & 0xff);	// Write data to LCD memory
		}
		break;
	}

	// inner square
	writeLCDControl(0x2A);		// Select Column Address
	writeLCDData(0);					// Setup beginning column address
	writeLCDData(x0+2);				// Setup beginning column address
	writeLCDData(0);					// Setup ending column address
	writeLCDData(x0+17);			// Setup ending column address
	writeLCDControl(0x2B);		// Select Row Address
	writeLCDData(y4);					// Setup beginning row address
	writeLCDData(y5);					// Setup beginning row address
	writeLCDData(y6);					// Setup ending row address
	writeLCDData(y7);					// Setup ending row address
	writeLCDControl(0x2C);		// Select Memory Write
	switch (pieceNumber) {
	case 1:
		for (i = 162; i > 0; i--)	{ 		// Loop through all memory locations 16 bit color
			writeLCDData(0xAC3F >> 8);		// Write data to LCD memory
			writeLCDData(0xAC3F & 0xff);	// Write data to LCD memory
			writeLCDData(0xAC3F >> 8);		// Write data to LCD memory
			writeLCDData(0xAC3F & 0xff);	// Write data to LCD memory
		}
		break;
	case 2:
		for (i = 162; i > 0; i--)	{ 		// Loop through all memory locations 16 bit color
			writeLCDData(0x7D7F >> 8);		// Write data to LCD memory
			writeLCDData(0x7D7F & 0xff);	// Write data to LCD memory
			writeLCDData(0x7D7F >> 8);		// Write data to LCD memory
			writeLCDData(0x7D7F & 0xff);	// Write data to LCD memory
		}
		break;
	case 3:
		for (i = 162; i > 0; i--)	{ 		// Loop through all memory locations 16 bit color
			writeLCDData(0x7EBF >> 8);		// Write data to LCD memory
			writeLCDData(0x7EBF & 0xff);	// Write data to LCD memory
			writeLCDData(0x7EBF >> 8);		// Write data to LCD memory
			writeLCDData(0x7EBF & 0xff);	// Write data to LCD memory
		}
		break;
	case 4:
		for (i = 162; i > 0; i--)	{ 		// Loop through all memory locations 16 bit color
			writeLCDData(0xAF5D >> 8);		// Write data to LCD memory
			writeLCDData(0xAF5D & 0xff);	// Write data to LCD memory
			writeLCDData(0xAF5D >> 8);		// Write data to LCD memory
			writeLCDData(0xAF5D & 0xff);	// Write data to LCD memory
		}
		break;
	case 5:
		for (i = 162; i > 0; i--)	{ 		// Loop through all memory locations 16 bit color
			writeLCDData(0x8CFF >> 8);		// Write data to LCD memory
			writeLCDData(0x8CFF & 0xff);	// Write data to LCD memory
			writeLCDData(0x8CFF >> 8);		// Write data to LCD memory
			writeLCDData(0x8CFF & 0xff);	// Write data to LCD memory
		}
		break;
	case 6:
		for (i = 162; i > 0; i--)	{ 		// Loop through all memory locations 16 bit color
			writeLCDData(0xD37C >> 8);		// Write data to LCD memory
			writeLCDData(0xD37C & 0xff);	// Write data to LCD memory
			writeLCDData(0xD37C >> 8);		// Write data to LCD memory
			writeLCDData(0xD37C & 0xff);	// Write data to LCD memory
		}
		break;
	case 7:
		for (i = 162; i > 0; i--)	{ 		// Loop through all memory locations 16 bit color
			writeLCDData(0xAEBB >> 8);		// Write data to LCD memory
			writeLCDData(0xAEBB & 0xff);	// Write data to LCD memory
			writeLCDData(0xAEBB >> 8);		// Write data to LCD memory
			writeLCDData(0xAEBB & 0xff);	// Write data to LCD memory
		}
		break;
	}
}

/***************************************************************************************
 * DRAW PIECE
 * 		Based on the piece and rotation, this draws the piece starting with the top left
 * 		location
 **************************************************************************************/
void drawPiece(int x, int y) {
	switch (piece) {
	case 1: // O
		drawSquare(x, y, piece);
		drawSquare(x+20, y, piece);
		drawSquare(x, y+20, piece);
		drawSquare(x+20, y+20, piece);
		break;
	case 2: // I
		switch (rotation) {
		case 0:
		case 2:
			drawSquare(x, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x, y+40, piece);
			drawSquare(x, y+60, piece);
			break;
		case 1:
		case 3:
			drawSquare(x, y, piece);
			drawSquare(x+20, y, piece);
			drawSquare(x+40, y, piece);
			drawSquare(x+60, y, piece);
			break;
		}
		break;
	case 3: // Z
		switch (rotation) {
		case 0:
		case 2:
			drawSquare(x, y, piece);
			drawSquare(x+20, y, piece);
			drawSquare(x+20, y+20, piece);
			drawSquare(x+40, y+20, piece);
			break;
		case 1:
		case 3:
			drawSquare(x+20, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x+20, y+20, piece);
			drawSquare(x, y+40, piece);
			break;
		}
		break;
	case 4: // S
		switch (rotation) {
		case 0:
		case 2:
			drawSquare(x+20, y, piece);
			drawSquare(x+40, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x+20, y+20, piece);
			break;
		case 1:
		case 3:
			drawSquare(x, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x+20, y+20, piece);
			drawSquare(x+20, y+40, piece);
			break;
		}
		break;
	case 5: // J
		switch (rotation) {
		case 0:
			drawSquare(x+20, y, piece);
			drawSquare(x+20, y+20, piece);
			drawSquare(x, y+40, piece);
			drawSquare(x+20, y+40, piece);
			break;
		case 1:
			drawSquare(x, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x+20, y+20, piece);
			drawSquare(x+40, y+20, piece);
			break;
		case 2:
			drawSquare(x, y, piece);
			drawSquare(x+20, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x, y+40, piece);
			break;
		case 3:
			drawSquare(x, y, piece);
			drawSquare(x+20, y, piece);
			drawSquare(x+40, y, piece);
			drawSquare(x+40, y+20, piece);
			break;
		}
		break;
	case 6: // L
		switch (rotation) {
		case 0:
			drawSquare(x, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x, y+40, piece);
			drawSquare(x+20, y+40, piece);
			break;
		case 1:
			drawSquare(x, y, piece);
			drawSquare(x+20, y, piece);
			drawSquare(x+40, y, piece);
			drawSquare(x, y+20, piece);
			break;
		case 2:
			drawSquare(x, y, piece);
			drawSquare(x+20, y, piece);
			drawSquare(x+20, y+20, piece);
			drawSquare(x+20, y+40, piece);
			break;
		case 3:
			drawSquare(x+40, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x+20, y+20, piece);
			drawSquare(x+40, y+20, piece);
			break;
		}
		break;
	case 7: // T
		switch (rotation) {
		case 0:
			drawSquare(x+20, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x+20, y+20, piece);
			drawSquare(x+40, y+20, piece);
			break;
		case 1:
			drawSquare(x, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x+20, y+20, piece);
			drawSquare(x, y+40, piece);
			break;
		case 2:
			drawSquare(x, y, piece);
			drawSquare(x+20, y, piece);
			drawSquare(x+40, y, piece);
			drawSquare(x+20, y+20, piece);
			break;
		case 3:
			drawSquare(x+20, y, piece);
			drawSquare(x, y+20, piece);
			drawSquare(x+20, y+20, piece);
			drawSquare(x+20, y+40, piece);
			break;
		}
		break;
	}
}

/***************************************************************************************
 * ERASE PIECE
 * 		Based on the piece and rotation, this draws the background color starting with the
 * 		top left location, effectively erasing it
 **************************************************************************************/
void erasePiece(int x, int y) {
	switch (piece) {
	case 1: // O
		drawSquare(x, y, 0);
		drawSquare(x+20, y, 0);
		drawSquare(x, y+20, 0);
		drawSquare(x+20, y+20, 0);
		break;
	case 2: // I
		switch (rotation) {
		case 0:
		case 2:
			drawSquare(x, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x, y+40, 0);
			drawSquare(x, y+60, 0);
			break;
		case 1:
		case 3:
			drawSquare(x, y, 0);
			drawSquare(x+20, y, 0);
			drawSquare(x+40, y, 0);
			drawSquare(x+60, y, 0);
			break;
		}
		break;
	case 3: // Z
		switch (rotation) {
		case 0:
		case 2:
			drawSquare(x, y, 0);
			drawSquare(x+20, y, 0);
			drawSquare(x+20, y+20, 0);
			drawSquare(x+40, y+20, 0);
			break;
		case 1:
		case 3:
			drawSquare(x+20, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x+20, y+20, 0);
			drawSquare(x, y+40, 0);
			break;
		}
		break;
	case 4: // S
		switch (rotation) {
		case 0:
		case 2:
			drawSquare(x+20, y, 0);
			drawSquare(x+40, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x+20, y+20, 0);
			break;
		case 1:
		case 3:
			drawSquare(x, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x+20, y+20, 0);
			drawSquare(x+20, y+40, 0);
			break;
		}
		break;
	case 5: // J
		switch (rotation) {
		case 0:
			drawSquare(x+20, y, 0);
			drawSquare(x+20, y+20, 0);
			drawSquare(x, y+40, 0);
			drawSquare(x+20, y+40, 0);
			break;
		case 1:
			drawSquare(x, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x+20, y+20, 0);
			drawSquare(x+40, y+20, 0);
			break;
		case 2:
			drawSquare(x, y, 0);
			drawSquare(x+20, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x, y+40, 0);
			break;
		case 3:
			drawSquare(x, y, 0);
			drawSquare(x+20, y, 0);
			drawSquare(x+40, y, 0);
			drawSquare(x+40, y+20, 0);
			break;
		}
		break;
	case 6: // L
		switch (rotation) {
		case 0:
			drawSquare(x, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x, y+40, 0);
			drawSquare(x+20, y+40, 0);
			break;
		case 1:
			drawSquare(x, y, 0);
			drawSquare(x+20, y, 0);
			drawSquare(x+40, y, 0);
			drawSquare(x, y+20, 0);
			break;
		case 2:
			drawSquare(x, y, 0);
			drawSquare(x+20, y, 0);
			drawSquare(x+20, y+20, 0);
			drawSquare(x+20, y+40, 0);
			break;
		case 3:
			drawSquare(x+40, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x+20, y+20, 0);
			drawSquare(x+40, y+20, 0);
			break;
		}
		break;
	case 7: // T
		switch (rotation) {
		case 0:
			drawSquare(x+20, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x+20, y+20, 0);
			drawSquare(x+40, y+20, 0);
			break;
		case 1:
			drawSquare(x, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x+20, y+20, 0);
			drawSquare(x, y+40, 0);
			break;
		case 2:
			drawSquare(x, y, 0);
			drawSquare(x+20, y, 0);
			drawSquare(x+40, y, 0);
			drawSquare(x+20, y+20, 0);
			break;
		case 3:
			drawSquare(x+20, y, 0);
			drawSquare(x, y+20, 0);
			drawSquare(x+20, y+20, 0);
			drawSquare(x+20, y+40, 0);
			break;
		}
		break;
	}
}

/***************************************************************************************
 * DRAW GRID
 * 		Draws the entire grid, drawing the background color for empty squares and the
 * 		colors of the pieces that aren't empty.
 **************************************************************************************/
void drawGrid(void) {
	int i;
	int j;
	int dX;
	int dY = 30;

	// row
	for (i = 0; i < 14; i++) {
		if (i != 0)
			dY += 20;
		// column
		for (j = 0; j < 10; j++) {
			dX = 20*j;
			drawSquare(dX+20, dY, grid[i][j]);
		}
	}
}

/***************************************************************************************
 * SET LEVEL COLOR
 * 		Sets the level color based on the level. levelColor is shown at the top left and
 * 		used when incrementing the score in the top right.
 **************************************************************************************/
void setLevelColor(void) {
	switch (level) {
	case 1:
	case 8:
		levelColor = 0xAEBB;
		break;
	case 2:
	case 9:
		levelColor = 0xAF5D;
		break;
	case 3:
	case 10:
		levelColor = 0x7EBF;
		break;
	case 4:
	case 11:
		levelColor = 0x7D7F;
		break;
	case 5:
	case 12:
		levelColor = 0x8CFF;
		break;
	case 6:
		levelColor = 0xAC3F;
		break;
	case 7:
		levelColor = 0xD37C;
		break;
	}
}

/***************************************************************************************
 * DRAW LEVEL COLOR
 * 		Draws the level color at the top left of the screen
 **************************************************************************************/
void drawLevelColor(void) {
	int i;

	writeLCDControl(0x2A);		// Select Column Address
	writeLCDData(0);					// Setup beginning column address
	writeLCDData(20);					// Setup beginning column address
	writeLCDData(0);					// Setup ending column address
	writeLCDData(40);					// Setup ending column address
	writeLCDControl(0x2B);		// Select Row Address
	writeLCDData(0);					// Setup beginning row address
	writeLCDData(5);					// Setup beginning row address
	writeLCDData(0);					// Setup ending row address
	writeLCDData(25);					// Setup ending row address
	writeLCDControl(0x2C);		// Select Memory Write
	for (i = 231; i > 0; i--) {					// Loop through all memory locations 16 bit color
		writeLCDData(levelColor >> 8);		// Write data to LCD memory
		writeLCDData(levelColor & 0xff);	// Write data to LCD memory
		writeLCDData(levelColor >> 8);		// Write data to LCD memory
		writeLCDData(levelColor & 0xff);	// Write data to LCD memory
	}
}

/***************************************************************************************
 * DRAW SCORE
 * 		Prints a 2x2 pixel point based on the level color and the shift given by lines
 * 		cleared and how many decades of lines cleared
 **************************************************************************************/
void drawScore(void) {
	drawPixel(218-scoreColumn, 23-scoreRow, levelColor);
	drawPixel(218-scoreColumn, 24-scoreRow, levelColor);
	drawPixel(219-scoreColumn, 23-scoreRow, levelColor);
	drawPixel(219-scoreColumn, 24-scoreRow, levelColor);
}

/***************************************************************************************
 * INITIALIZE BACKGROUND
 * 		Draws the main UI. This consists of a light blue background and darker blue
 * 		foreground, with a small amount of shade underneath it
 **************************************************************************************/
void initBackground(void) {
	int i;
	fillScreen(0x5B57);

	writeLCDControl(0x2A);		// Select Column Address
	writeLCDData(0);					// Setup beginning column address
	writeLCDData(0x11);				// Setup beginning column address
	writeLCDData(0);					// Setup ending column address
	writeLCDData(0xDE);				// Setup ending column address
	writeLCDControl(0x2B);		// Select Row Address
	writeLCDData(0);					// Setup beginning row address
	writeLCDData(0x21);				// Setup beginning row address
	writeLCDData(0x01);				// Setup ending row address
	writeLCDData(0x38);				// Setup ending row address
	writeLCDControl(0x2C);		// Select Memory Write
	for (i = 207 * 141; i > 0; i--)	{ 	// Loop through all memory locations 16 bit color
		writeLCDData(0x5316 >> 8);				// Write data to LCD memory
		writeLCDData(0x5316 & 0xff);			// Write data to LCD memory
		writeLCDData(0x5316 >> 8);				// Write data to LCD memory
		writeLCDData(0x5316 & 0xff);			// Write data to LCD memory
	}

	writeLCDControl(0x2A);		// Select Column Address
	writeLCDData(0);					// Setup beginning column address
	writeLCDData(0x12);				// Setup beginning column address
	writeLCDData(0);					// Setup ending column address
	writeLCDData(0xDD);				// Setup ending column address
	writeLCDControl(0x2B);		// Select Row Address
	writeLCDData(0);					// Setup beginning row address
	writeLCDData(0x20);				// Setup beginning row address
	writeLCDData(0x01);				// Setup ending row address
	writeLCDData(0x37);				// Setup ending row address
	writeLCDControl(0x2C);		// Select Memory Write
	for (i = 205 * 141; i > 0; i--)	{		// Loop through all memory locations 16 bit color
		writeLCDData(0x4AF4 >> 8);				// Write data to LCD memory
		writeLCDData(0x4AF4 & 0xff);			// Write data to LCD memory
		writeLCDData(0x4AF4 >> 8);				// Write data to LCD memory
		writeLCDData(0x4AF4 & 0xff);			// Write data to LCD memory
	}

	writeLCDControl(0x2A);		// Select Column Address
	writeLCDData(0);					// Setup beginning column address
	writeLCDData(0x13);				// Setup beginning column address
	writeLCDData(0);					// Setup ending column address
	writeLCDData(0xDC);				// Setup ending column address
	writeLCDControl(0x2B);		// Select Row Address
	writeLCDData(0);					// Setup beginning row address
	writeLCDData(0x1F);				// Setup beginning row address
	writeLCDData(0x01);				// Setup ending row address
	writeLCDData(0x36);				// Setup ending row address
	writeLCDControl(0x2C);		// Select Memory Write
	for (i = 203 * 141; i > 0; i--)	{ 	// Loop through all memory locations 16 bit color
		writeLCDData(0x42B2 >> 8);				// Write data to LCD memory
		writeLCDData(0x42B2 & 0xff);			// Write data to LCD memory
		writeLCDData(0x42B2 >> 8);				// Write data to LCD memory
		writeLCDData(0x42B2 & 0xff);			// Write data to LCD memory
	}

	writeLCDControl(0x2A);		// Select Column Address
	writeLCDData(0);					// Setup beginning column address
	writeLCDData(0x14);				// Setup beginning column address
	writeLCDData(0);					// Setup ending column address
	writeLCDData(0xDB);				// Setup ending column address
	writeLCDControl(0x2B);		// Select Row Address
	writeLCDData(0);					// Setup beginning row address
	writeLCDData(0x1E);				// Setup beginning row address
	writeLCDData(0x01);				// Setup ending row address
	writeLCDData(0x35);				// Setup ending row address
	writeLCDControl(0x2C);		// Select Memory Write
	for (i = 201 * 141; i > 0; i--)	{ 	// Loop through all memory locations 16 bit color
		writeLCDData(0x110F >> 8);				// Write data to LCD memory
		writeLCDData(0x110F & 0xff);			// Write data to LCD memory
		writeLCDData(0x110F >> 8);				// Write data to LCD memory
		writeLCDData(0x110F & 0xff);			// Write data to LCD memory
	}
}

/***************************************************************************************
 * READ TOUCHSCREEN
 * 		Reads the Z value (or how hard a press is) on the touchscreen. Takes care of all
 * 		all the initiation of the ADC and hooks it to P1.4 ADC10MEM is then read.
 **************************************************************************************/
void readTS(void) {
	unsigned int z0;
	unsigned int z1;

	// read z value
	P1DIR &= ~TS_YP;
	P1DIR &= ~TS_XM;
	P1DIR |= TS_XP + TS_YM;

	ADC10CTL0 = ADC10SHT_2 + ADC10ON;
	ADC10CTL1 = INCH_1;
	ADC10AE0 = BIT1;

	P1OUT |= TS_YM;
	P1OUT &= ~TS_XM;

	ADC10CTL0 |= ENC + ADC10SC;
	while (ADC10CTL1 & 0x0001);
	z0 = ADC10MEM;

	P1OUT = 0;

	P1DIR &= ~TS_YP;
	P1DIR &= ~TS_XM;
	P1DIR |= TS_XP + TS_YM;

	ADC10CTL0 = ADC10SHT_2 + ADC10ON;
	ADC10CTL1 = INCH_0;
	ADC10AE0 = BIT0;

	P1OUT |= TS_YM;
	P1OUT &= ~TS_XM;

	ADC10CTL0 |= ENC + ADC10SC;
	while (ADC10CTL1 & 0x0001);
	z1 = ADC10MEM;

	z = 1023 - z0 + z1;
}

/***************************************************************************************
 * INITIALIZE CLOCK
 * 		Set the clock to the maximum of 20MHz
 **************************************************************************************/
void initClk(void) {
	BCSCTL1 = 0x8F;
	DCOCTL = 0xFF;
}

/***************************************************************************************
 * INITIALIZE PINS
 * 		Initializes all the pins for output when needed and sets interrupt flags
 **************************************************************************************/
void initPins(void) {
	P1DIR = LCD_MOSI | LCD_SCK;
	P2DIR = LCD_RST | LCD_CS | LCD_DC;
	// redundant but making sure
	P1DIR &= ~BIT4;
	P1DIR &= ~BIT6;
	P2DIR &= ~BIT3;
	P2DIR &= ~BIT4;

	P1SEL |= LCD_MOSI + LCD_SCK;
	P1SEL2 |= LCD_MOSI + LCD_SCK;

	P2OUT = LCD_RST | LCD_CS | LCD_DC;

	P2IE |= BTN_LFT | BTN_RGHT;
	P2IES |= BTN_LFT | BTN_RGHT;
	P2IFG &= ~BTN_LFT;
	P2IFG &= ~BTN_RGHT;

	P1IE	|= BTN_ROT;
	P1IES	|= BTN_ROT;
	P1IFG &= ~BTN_ROT;

	_BIS_SR(GIE);
}

/***************************************************************************************
 * FILL SCREEN
 * 		Fills the entire screen with the given color.
 **************************************************************************************/
void fillScreen(int color) {
	unsigned int i;

	writeLCDControl(0x2A);		// Select Column Address
	writeLCDData(0);					// Setup beginning column address
	writeLCDData(0);					// Setup beginning column address
	writeLCDData(0);					// Setup ending column address
	writeLCDData(239);				// Setup ending column address
	writeLCDControl(0x2B);		// Select Row Address
	writeLCDData(0);					// Setup beginning row address
	writeLCDData(0);					// Setup beginning row address
	writeLCDData(0x01);				// Setup ending row address
	writeLCDData(0x3F);				// Setup ending row address
	writeLCDControl(0x2C);		// Select Memory Write
	for (i = 240 * 161; i > 0; i--) {		// Loop through all memory locations 16 bit color
		writeLCDData(color >> 8);					// Write data to LCD memory
		writeLCDData(color & 0xff);				// Write data to LCD memory
		writeLCDData(color >> 8);					// Write data to LCD memory
		writeLCDData(color & 0xff);				// Write data to LCD memory
	}
}

/***************************************************************************************
 * DRAW PIXEL
 * 		Draws one pixel given and x, y, and color value
 **************************************************************************************/
void drawPixel(int x, int y, int color) {
	writeLCDControl(0x2A);				// Select Column Address
	writeLCDData(x >> 8);					// Starting x Address (Most Sig 8 bits of address)
	writeLCDData(x & 0xff);				// Starting x Address (Least Sig 8 bits of address)
	writeLCDData(x >> 8);					// Ending x Address (Most Sig 8 bits of address)
	writeLCDData(x & 0xff);				// Ending x Address (Least Sig 8 bits of address)
	writeLCDControl(0x2B);				// Select Row Address
	writeLCDData(y >> 8);					// Starting y Address (Most Sig 8 bits of address)
	writeLCDData(y & 0xff);				// Starting y Address (Least Sig 8 bits of address)
	writeLCDData(y >> 8);					// Ending y Address (Most Sig 8 bits of address)
	writeLCDData(y & 0xff);				// Ending y Address (Least Sig 8 bits of address)
	writeLCDControl(0x2C);				// Select Color to write to the pixel
	writeLCDData(color >> 8);			// Send Most Significant 8 bits of 16 bit color
	writeLCDData(color & 0xff);		// Send Least Significant 8 bits of 16 bit color
}

/***************************************************************************************
 * WRITE LCD CONTROL
 **************************************************************************************/
void writeLCDControl(char data) {
	P2OUT &= ~LCD_DC;						// DC low
	P2OUT &= ~LCD_CS;						// CS Low

	while (IFG2 & UCB0TXIFG);		// TX buffer ready?
	UCB0TXBUF = data;						// start transmission

	return;
}

/***************************************************************************************
 * WRITE LCD DATA
 **************************************************************************************/
void writeLCDData(char data) {
	P2OUT |= LCD_DC;						// DC high
	P2OUT &= ~LCD_CS;						// CS Low

	while (IFG2 & UCB0TXIFG);		// TX buffer ready?
	UCB0TXBUF = data;						// start transmission

	return;
}

/***************************************************************************************
 * WAIT MILLISECOND
 * 		Waits a given amount of milliseconds
 **************************************************************************************/
void waitMS(unsigned int m_sec) {
	while (m_sec--)
		__delay_cycles(20000);			// 1000 for 1MHz
}

/***************************************************************************************
 * INITIALIZE LCD SCREEN
 * 		Initializes the lcd screen for all the things we need to do
 **************************************************************************************/
void initLCD(void) {
	P1OUT = 0x00;							// Set all outputs low for initialization
	P2OUT = LCD_RST | LCD_CS | LCD_DC;

	P2OUT |= LCD_RST;					// Reset LCD
	waitMS(10);
	P2OUT &= ~LCD_RST;
	waitMS(10);
	P2OUT |= LCD_RST;
	waitMS(100);

	writeLCDControl(0xEF);				//
	writeLCDData(0x03);
	writeLCDData(0x80);
	writeLCDData(0x02);
	writeLCDControl(0xCB);				// Power Control A
	writeLCDData(0x39);
	writeLCDData(0x2C);
	writeLCDData(0x00);
	writeLCDData(0x34);
	writeLCDData(0x02);
	writeLCDControl(0xCF);				// Power Control B
	writeLCDData(0x00);
	writeLCDData(0xC1);
	writeLCDData(0x30);
	writeLCDControl(0xED);				// Power On Sequence Control
	writeLCDData(0x64);
	writeLCDData(0x03);
	writeLCDData(0x12);
	writeLCDData(0x81);
	writeLCDControl(0xE8);				// Driver Timing Control A
	writeLCDData(0x85);
	writeLCDData(0x00);
	writeLCDData(0x78);
	writeLCDControl(0xF7);				// Pump Ration Control
	writeLCDData(0x20);
	writeLCDControl(0xEA);				// Driver Timing Control A
	writeLCDData(0x00);
	writeLCDData(0x00);
	writeLCDControl(0xC0); 				// Power Control 1
	writeLCDData(0x23);
	writeLCDControl(0xC1);    		// Power Control 2
	writeLCDData(0x10);
	writeLCDControl(0xC5);    		// VCOM Control
	writeLCDData(0x3e);
	writeLCDData(0x28);
	writeLCDControl(0xC7);    		// VCOM Control2
	writeLCDData(0x86);
	writeLCDControl(0x36);    		// Memory Access Control
	writeLCDData(0x40 | 0x08);
	writeLCDControl(0x3A);				// COLMOD Pixel Format Set
	writeLCDData(0x55);
	writeLCDControl(0xB1);				// Frame Rate Control
	writeLCDData(0x00);
	writeLCDData(0x18);
	writeLCDControl(0xB6);   			// Display Function Control
	writeLCDData(0x08);
	writeLCDData(0x82);
	writeLCDData(0x27);
	writeLCDControl(0xF2);   			// 3Gamma Control
	writeLCDData(0x00);
	writeLCDControl(0x26);    		// Gamma curve selected
	writeLCDData(0x01);
	writeLCDControl(0xE0);   			// Positive Gamma Correction
	writeLCDData(0x0F);
	writeLCDData(0x31);
	writeLCDData(0x2B);
	writeLCDData(0x0C);
	writeLCDData(0x0E);
	writeLCDData(0x08);
	writeLCDData(0x4E);
	writeLCDData(0xF1);
	writeLCDData(0x37);
	writeLCDData(0x07);
	writeLCDData(0x10);
	writeLCDData(0x03);
	writeLCDData(0x0E);
	writeLCDData(0x09);
	writeLCDData(0x00);
	writeLCDControl(0xE1);    		// Negative Gamma Correction
	writeLCDData(0x00);
	writeLCDData(0x0E);
	writeLCDData(0x14);
	writeLCDData(0x03);
	writeLCDData(0x11);
	writeLCDData(0x07);
	writeLCDData(0x31);
	writeLCDData(0xC1);
	writeLCDData(0x48);
	writeLCDData(0x08);
	writeLCDData(0x0F);
	writeLCDData(0x0C);
	writeLCDData(0x31);
	writeLCDData(0x36);
	writeLCDData(0x0F);
	writeLCDControl(0x11);    		// Exit Sleep
	writeLCDControl(0x29);    		// Display on
}

/***************************************************************************************
 * DRAW INSTRUCTION
 * 		Draws the instructions to start the game "tap to start"
 **************************************************************************************/
void drawInstruction(char x0, char y0, int color) {
	drawLetter(0x74, x0, y0, color);			// t
	drawLetter(0x61, x0+4, y0, color);		// a
	drawLetter(0x70, x0+12, y0, color);		// p

	drawLetter(0x74, x0+26, y0, color);		// t
	drawLetter(0x6F, x0+30, y0, color);		// o

	drawLetter(0x73, x0+44, y0, color);		// s
	drawLetter(0x74, x0+51, y0, color);		// t
	drawLetter(0x61, x0+55, y0, color);		// a
	drawLetter(0x72, x0+63, y0, color);		// r
	drawLetter(0x74, x0+69, y0, color);		// t
}

/***************************************************************************************
 * DRAW LETTER
 * 		Draws a letter based off what value is based, the x and y value of the top left
 * 		coordinate and the color of the text
 **************************************************************************************/
void drawLetter(char data, char x0, char y0, int color) {
	switch (data) {
		case 0x61: // a
			drawPixel(x0+1, y0+4, color);
			drawPixel(x0+1, y0+7, color);
			drawPixel(x0+1, y0+8, color);
			drawPixel(x0+2, y0+3, color);
			drawPixel(x0+2, y0+6, color);
			drawPixel(x0+2, y0+9, color);
			drawPixel(x0+3, y0+3, color);
			drawPixel(x0+3, y0+6, color);
			drawPixel(x0+3, y0+9, color);
			drawPixel(x0+4, y0+3, color);
			drawPixel(x0+4, y0+6, color);
			drawPixel(x0+4, y0+9, color);
			drawPixel(x0+5, y0+4, color);
			drawPixel(x0+5, y0+5, color);
			drawPixel(x0+5, y0+6, color);
			drawPixel(x0+5, y0+7, color);
			drawPixel(x0+5, y0+8, color);
			drawPixel(x0+6, y0+9, color);
			break;
		case 0x6F: // o
			drawPixel(x0+1, y0+5, color);
			drawPixel(x0+1, y0+6, color);
			drawPixel(x0+1, y0+7, color);
			drawPixel(x0+2, y0+4, color);
			drawPixel(x0+2, y0+8, color);
			drawPixel(x0+3, y0+3, color);
			drawPixel(x0+3, y0+9, color);
			drawPixel(x0+4, y0+3, color);
			drawPixel(x0+4, y0+9, color);
			drawPixel(x0+5, y0+4, color);
			drawPixel(x0+5, y0+8, color);
			drawPixel(x0+6, y0+5, color);
			drawPixel(x0+6, y0+6, color);
			drawPixel(x0+6, y0+7, color);
			break;
		case 0x70: // p
			drawPixel(x0+1, y0+3, color);
			drawPixel(x0+1, y0+4, color);
			drawPixel(x0+1, y0+5, color);
			drawPixel(x0+1, y0+6, color);
			drawPixel(x0+1, y0+7, color);
			drawPixel(x0+1, y0+8, color);
			drawPixel(x0+1, y0+9, color);
			drawPixel(x0+1, y0+10, color);
			drawPixel(x0+1, y0+11, color);
			drawPixel(x0+2, y0+4, color);
			drawPixel(x0+2, y0+8, color);
			drawPixel(x0+3, y0+3, color);
			drawPixel(x0+3, y0+9, color);
			drawPixel(x0+4, y0+3, color);
			drawPixel(x0+4, y0+9, color);
			drawPixel(x0+5, y0+4, color);
			drawPixel(x0+5, y0+8, color);
			drawPixel(x0+6, y0+5, color);
			drawPixel(x0+6, y0+6, color);
			drawPixel(x0+6, y0+7, color);
			break;
		case 0x72: // r
			drawPixel(x0+1, y0+3, color);
			drawPixel(x0+1, y0+4, color);
			drawPixel(x0+1, y0+5, color);
			drawPixel(x0+1, y0+6, color);
			drawPixel(x0+1, y0+7, color);
			drawPixel(x0+1, y0+8, color);
			drawPixel(x0+1, y0+9, color);
			drawPixel(x0+2, y0+4, color);
			drawPixel(x0+3, y0+3, color);
			break;
		case 0x73: // s
			drawPixel(x0+1, y0+4, color);
			drawPixel(x0+1, y0+5, color);
			drawPixel(x0+1, y0+8, color);
			drawPixel(x0+2, y0+3, color);
			drawPixel(x0+2, y0+6, color);
			drawPixel(x0+2, y0+9, color);
			drawPixel(x0+3, y0+3, color);
			drawPixel(x0+3, y0+6, color);
			drawPixel(x0+3, y0+9, color);
			drawPixel(x0+4, y0+4, color);
			drawPixel(x0+4, y0+6, color);
			drawPixel(x0+4, y0+9, color);
			drawPixel(x0+5, y0+7, color);
			drawPixel(x0+5, y0+8, color);
			break;
		case 0x74: // t
			drawPixel(x0, y0+2, color);
			drawPixel(x0+1, y0, color);
			drawPixel(x0+1, y0+1, color);
			drawPixel(x0+1, y0+2, color);
			drawPixel(x0+1, y0+3, color);
			drawPixel(x0+1, y0+4, color);
			drawPixel(x0+1, y0+5, color);
			drawPixel(x0+1, y0+6, color);
			drawPixel(x0+1, y0+7, color);
			drawPixel(x0+1, y0+8, color);
			drawPixel(x0+2, y0+2, color);
			drawPixel(x0+2, y0+9, color);
			break;
	}
}

/***************************************************************************************
 * INITIALIZE USCI
 **************************************************************************************/
void initUSCI(void) {
	UCB0CTL1 |= UCSWRST;					// USCI in reset state
	UCB0CTL0 |= UCMST + UCSYNC + UCCKPH + UCMSB;// SPI Master, 8bit, MSB first, synchronous mode
	UCB0CTL1 |= UCSSEL_2;					// USCI CLK-SRC=SMCLK=~8MHz
	UCB0CTL1 &= ~UCSWRST;					// USCI released for operation
	IE2 |= UCB0TXIE;							// enable TX interrupt
	IFG2 &= ~UCB0TXIFG;

	_EINT();											// enable interrupts
}

/***************************************************************************************
 * INTERRUPT USCI
 **************************************************************************************/
#pragma INTERRUPT (USCI)
#pragma vector=USCIAB0TX_VECTOR
void USCI(void) {
	P2OUT |= LCD_CS;						// transmission done
	IFG2 &= ~UCB0TXIFG;					// clear TXIFG
}

/***************************************************************************************
 * INTERRUPT PORT1
 * 		Interrupt happens when the rotate key gets pressed. It sets the rotate key
 * 		variable to true.
 **************************************************************************************/
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void) {
	rotateKey = true;
	keyPress += 18;
	P1IFG &= ~BTN_ROT;
	P2IFG &= ~BTN_LFT;
	P2IFG &= ~BTN_RGHT;
}

/***************************************************************************************
 * INTERRUPT PORT2
 * 		Interrupt happens when the left or right key gets pressed. It sets the left or
 * 		right key variable to true, accordingly, based off what pin is turned on. (It's
 * 		actually wired backwards because I forgot that the buttons are high asserted. It
 * 		works regardless).
 **************************************************************************************/
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void) {
	if (P2IN & BTN_RGHT) {
		rightKey = true;
		keyPress += 33;
	}	else if (P2IN & BTN_LFT) {
		leftKey = true;
		keyPress += 29;
	}
	P1IFG &= ~BTN_ROT;
	P2IFG &= ~BTN_LFT;
	P2IFG &= ~BTN_RGHT;
}
