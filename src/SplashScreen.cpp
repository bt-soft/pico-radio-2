#include "SplashScreen.h"

#include "defines.h"

// Program konstansok
const char* SplashScreen::APP_NAME = PROGRAM_NAME;
const char* SplashScreen::APP_VERSION = PROGRAM_VERSION;
const char* SplashScreen::APP_AUTHOR = PROGRAM_AUTHOR;

SplashScreen::SplashScreen(TFT_eSPI& tft, SI4735& si4735) : tft(tft), si4735(si4735) {}

void SplashScreen::show(bool showProgress, uint8_t progressSteps) {
    // Képernyő törlése
    tft.fillScreen(BACKGROUND_COLOR);

    // Elemek kirajzolása
    drawBorder();
    drawTitle();
    drawSI4735Info();
    drawBuildInfo();
    drawProgramInfo();

    if (showProgress) {
        drawProgressBar(0);
    }
}

void SplashScreen::drawBorder() {
    // Díszes keret rajzolása
    int16_t w = tft.width();
    int16_t h = tft.height();

    // Külső keret
    tft.drawRect(2, 2, w - 4, h - 4, BORDER_COLOR);
    tft.drawRect(3, 3, w - 6, h - 6, BORDER_COLOR);

    // Sarkok díszítése
    for (int i = 0; i < 8; i++) {
        tft.drawPixel(5 + i, 5, BORDER_COLOR);
        tft.drawPixel(5, 5 + i, BORDER_COLOR);

        tft.drawPixel(w - 6 - i, 5, BORDER_COLOR);
        tft.drawPixel(w - 6, 5 + i, BORDER_COLOR);

        tft.drawPixel(5 + i, h - 6, BORDER_COLOR);
        tft.drawPixel(5, h - 6 - i, BORDER_COLOR);

        tft.drawPixel(w - 6 - i, h - 6, BORDER_COLOR);
        tft.drawPixel(w - 6, h - 6 - i, BORDER_COLOR);
    }
}

void SplashScreen::drawTitle() {
    tft.setFreeFont();
    tft.setTextSize(2);
    tft.setTextColor(TITLE_COLOR, BACKGROUND_COLOR);
    tft.setTextDatum(TC_DATUM);  // Felső közép igazítás

    int16_t titleY = 20;
    tft.drawString(APP_NAME, tft.width() / 2, titleY);

    // Aláhúzás
    int16_t textWidth = tft.textWidth(APP_NAME);
    int16_t lineY = titleY + tft.fontHeight() + 2;
    int16_t lineStartX = (tft.width() - textWidth) / 2;
    int16_t lineEndX = lineStartX + textWidth;

    for (int i = 0; i < 3; i++) {
        tft.drawLine(lineStartX, lineY + i, lineEndX, lineY + i, TITLE_COLOR);
    }
}

void SplashScreen::drawSI4735Info() {
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);  // Bal felső igazítás

    uint16_t startY = 70;
    uint16_t leftX = 15;
    uint16_t rightX = 180;
    uint8_t lineHeight = 20;
    uint16_t currentY = startY;

    // SI4735 címsor
    tft.setTextColor(INFO_LABEL_COLOR, BACKGROUND_COLOR);
    tft.drawString("SI4735 Firmware:", leftX, currentY);
    currentY += lineHeight + 5;

    // SI4735 adatok
    tft.setTextColor(INFO_VALUE_COLOR, BACKGROUND_COLOR);

    // Part Number
    tft.drawString("Part Number:", leftX, currentY);
    tft.drawString("0x" + String(si4735.getFirmwarePN(), HEX), rightX, currentY);
    currentY += lineHeight;

    // Firmware verzió
    tft.drawString("Firmware:", leftX, currentY);
    tft.drawString(String(si4735.getFirmwareFWMAJOR()) + "." + String(si4735.getFirmwareFWMINOR()), rightX, currentY);
    currentY += lineHeight;

    // Patch ID
    tft.drawString("Patch ID:", leftX, currentY);
    String patchHex = String(si4735.getFirmwarePATCHH(), HEX);
    if (patchHex.length() == 1) patchHex = "0" + patchHex;
    String patchLowHex = String(si4735.getFirmwarePATCHL(), HEX);
    if (patchLowHex.length() == 1) patchLowHex = "0" + patchLowHex;
    tft.drawString("0x" + patchHex + patchLowHex, rightX, currentY);
    currentY += lineHeight;

    // Component verzió
    tft.drawString("Component:", leftX, currentY);
    tft.drawString(String(si4735.getFirmwareCMPMAJOR()) + "." + String(si4735.getFirmwareCMPMINOR()), rightX, currentY);
    currentY += lineHeight;

    // Chip Rev
    tft.drawString("Chip Rev:", leftX, currentY);
    tft.drawString(String(si4735.getFirmwareCHIPREV()), rightX, currentY);
}

void SplashScreen::drawBuildInfo() {
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);

    uint16_t startY = 200;
    uint16_t leftX = 15;
    uint16_t rightX = 120;
    uint8_t lineHeight = 16;
    uint16_t currentY = startY;

    // Build címsor
    tft.setTextColor(INFO_LABEL_COLOR, BACKGROUND_COLOR);
    tft.drawString("Build Information:", leftX, currentY);
    currentY += lineHeight + 3;

    tft.setTextColor(INFO_VALUE_COLOR, BACKGROUND_COLOR);

    // Dátum
    tft.drawString("Date:", leftX, currentY);
    tft.drawString(__DATE__, rightX, currentY);
    currentY += lineHeight;

    // Idő
    tft.drawString("Time:", leftX, currentY);
    tft.drawString(__TIME__, rightX, currentY);
}

void SplashScreen::drawProgramInfo() {
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(BC_DATUM);  // Alsó közép igazítás

    uint16_t bottomY = tft.height() - 50;

    // Program verzió
    tft.setTextColor(VERSION_COLOR, BACKGROUND_COLOR);
    tft.drawString("Version " + String(APP_VERSION), tft.width() / 2, bottomY);

    // Szerző
    tft.setTextColor(AUTHOR_COLOR, BACKGROUND_COLOR);
    tft.drawString(APP_AUTHOR, tft.width() / 2, bottomY + 15);
}

void SplashScreen::drawProgressBar(uint8_t progress) {
    int16_t barWidth = 200;
    int16_t barHeight = 8;
    int16_t barX = (tft.width() - barWidth) / 2;
    int16_t barY = tft.height() - 25;

    // Progress bar háttér
    tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, TFT_WHITE);
    tft.fillRect(barX, barY, barWidth, barHeight, TFT_BLACK);

    // Progress kitöltés
    if (progress > 0) {
        int16_t fillWidth = (barWidth * progress) / 100;
        tft.fillRect(barX, barY, fillWidth, barHeight, TFT_GREEN);
    }
}

void SplashScreen::updateProgress(uint8_t step, uint8_t totalSteps, const char* message) {
    uint8_t progress = (step * 100) / totalSteps;
    drawProgressBar(progress);

    if (message != nullptr) {
        // Előző üzenet törlése
        tft.fillRect(0, tft.height() - 45, tft.width() - 10, 15, BACKGROUND_COLOR);

        // Új üzenet kiírása
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextColor(TFT_YELLOW, BACKGROUND_COLOR);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(message, tft.width() / 2, tft.height() - 45);
    }
}

void SplashScreen::hide() { tft.fillScreen(TFT_BLACK); }
