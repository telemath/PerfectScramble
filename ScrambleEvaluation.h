#pragma once

constexpr auto FACE_ARRANGEMENTS = 10077696; // CUBE_COLORS^9, every possible arrangement of a face. 

constexpr auto CUBE_SURFACES = 54;    // 54 visible surfaces on a 3x3 Rubik's cube.
constexpr auto CUBE_FACES = 6;        // 6 faces on a cube.
constexpr auto CUBE_CORNERS = 8;      // 8 corner pieces on a cube.
constexpr auto CUBE_EDGES = 12;       // 12 edge pieces on a cube.
constexpr auto CUBE_COLORS = 6;       // 6 colors on a cube.

#define SWAP(x, y) {unsigned char temp = x; x = y; y = temp;}

// Color connectedness
constexpr auto SIDES_TOUCHING = 0;          // Two surfaces of the same color are touching on a side.
constexpr auto CORNERS_TOUCHING = 1;        // Two surfaces of the same color are touching at the corners.
constexpr auto ADJACENT_FACES_TOUCHING = 2; // Two surfaces of the same color, on adjacent faces, are touching at the corners.
constexpr auto NOTHING_TOUCHING = 3;        // None of the above are touching.

extern __int16 face_table[FACE_ARRANGEMENTS]; // The unique pattern id for every possible face arrangment.

void BuildFaceTable();
// These write the face table out to a file and then read them for the next time the program is run.
// They're really not necessary since building the face table from scratch is about as fast as reading it from a file.
bool ReadFaceTable();
bool WriteFaceTable();

// See how connected a cube is.
int GetColorConnectedness(unsigned char cube[CUBE_SURFACES]);
