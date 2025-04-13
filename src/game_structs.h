#ifndef GAME_STRUCTS_H
#define GAME_STRUCTS_H

struct Location {
    int x;
    int y;
};

#endif // GAME_STRUCTS_H

// Structure to represent a background tile
struct BackgroundTile {
    Texture2D texture;
    Vector2 position;
};
