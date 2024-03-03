#include "ScrambleEvaluation.h"
#include "stdio.h"
#include "stdlib.h"

#define __SANITY_CHECKS__

const __int16 NOT_SET = 32767;

__int16 face_table[FACE_ARRANGEMENTS]; // The unique pattern id for every possible face arrangment.
bool face_table_filled = false;


////////////////////////////////////////////////////////////////////////////////
// Scramble criteria
////////////////////////////////////////////////////////////////////////////////


// Count the number of different colors on a face and the max number of instances of any single color.
// Face colors[i] must be a value from 0 to 5.
void GetFaceColorCounts(unsigned char face_colors[9], unsigned char* count, unsigned char* max_instances)
{
    int color_counts[CUBE_COLORS] = { 0, 0, 0, 0, 0, 0 };
    int color;
    *count = 0;
    *max_instances = 0;

    for (int i = 0; i < 9; ++i) {
        color = face_colors[i];
        if (0 == color_counts[color]) {
            ++* count;
        }
        ++color_counts[color];
        if (*max_instances < color_counts[color]) {
            *max_instances = color_counts[color];
        }
    }
}


// Get the color connectedness for a single face.
// Face colors[i] must be a value from 0 to 5.
unsigned char GetFaceColorConnectedness(unsigned char face_colors[9])
{
    // A cube face is laid out like this:
    //        +-+-+-+
    //        |0|1|2|
    //        +-+-+-+
    //        |3|4|5|
    //        +-+-+-+
    //        |6|7|8|

    // A list of pairs of spots in a cube face to compare to see if two vertically or horizontally adjacent spots are the same color.
    // E.g. sides_to_compare[0] = {0, 1}, which indicates that we should compare spot 0 to spot 1 to see if they're the same color.
    unsigned int sides_to_compare[12][2] = { {0, 1}, {1, 2}, {3, 4}, {4, 5}, {6, 7}, {7, 8},
                                             {0, 3}, {3, 6}, {1, 4}, {4, 7}, {2, 5}, {5, 8} };

    // A list of pairs of spots in a cube face to compare to see if two diagonally adjacent spots are the same color.
    // E.g. sides_to_compare[0] = {0, 4}, which indicates that we should compare spot 0 to spot 4 to see if they're the same color.
    unsigned int corners_to_compare[8][2] = { {0, 4}, {2, 4}, {6, 4}, {8, 4},
                                              {1, 3}, {3, 7}, {7, 5}, {5, 1} };

    // Compare side-by-side spots. If any are the same color, the connectedness is SIDES_TOUCHING.
    for (int i = 0; i < 12; ++i) {
        if (face_colors[sides_to_compare[i][0]] == face_colors[sides_to_compare[i][1]]) {
            return SIDES_TOUCHING;
        }
    }

    // Compare diagonal spots. If any are the same color, the connectedness is CORNERS_TOUCHING.
    for (int i = 0; i < 8; ++i) {
        if (face_colors[corners_to_compare[i][0]] == face_colors[corners_to_compare[i][1]]) {
            return CORNERS_TOUCHING;
        }
    }

    return NOTHING_TOUCHING;
}


////////////////////////////////////////////////////////////////////////////////
// Color Pattern Ids - A unique id for every possible pattern on a single face.
////////////////////////////////////////////////////////////////////////////////


// Convert the index into face_table[] into a list of colors on a cube face.
void FaceIndexToColors(int index, unsigned char colors[9])
{
    for (int i = 0; i < 9; ++i) {
        colors[i] = index % CUBE_COLORS;
        index /= CUBE_COLORS;
    }
}


// Build the face table.
// 
// A face is converted to an id by treading the colors of each face as a 9-digit, base-6 number.
// Swapping colors, or flipping or rotating that face will produce a different id, but all
// variations of a single face id will have the same value in the face table.
// Thus, face_table is used to reduce a collection of colors on a face to its lowest id.
// 
// Patterns 0-15 are "perfect shuffle" patterns - all 6 colors on one face, no more
// than two of any color, and no two of the same color touching on an edge or corner.
void BuildFaceTable() {
    unsigned char symmetries[8][9] =
    { { 0, 1, 2, 3, 4, 5, 6, 7, 8},   // Original face.
      { 2, 5, 8, 1, 4, 7, 0, 3, 6},   // Rotated 90 degrees counter     clockwise.
      { 8, 7, 6, 5, 4, 3, 2, 1, 0},   // Rotated 180 degrees.
      { 6, 3, 0, 7, 4, 1, 8, 5, 2},   // Rotated 90 degrees clockwise.
      { 2, 1, 0, 5, 4, 3, 8, 7, 6},   // Flipped.
      { 8, 5, 2, 7, 4, 1, 6, 3, 0},   // Flipped, rotated 90 degrees clockwise.
      { 6, 7, 8, 3, 4, 5, 0, 1, 2},   // Flipped, rotated 180 degrees.
      { 0, 3, 6, 1, 4, 7, 2, 5, 8} }; // Flipped, rotated 90 degrees counter-clockwise.

    // Mark all patterns as unset.
    for (int i = 0; i < FACE_ARRANGEMENTS; ++i) {
        face_table[i] = NOT_SET;
    }

    // A perfect face pattern has:
    //     1. All 6 colors on one face.
    //     2. No more than two surfaces of each color.
    //     3. No two surfaces of the same color touching on an edge.
    //     4. No two surfaces of the same color touching on a diagonal.
    // There are 16 perfect patterns, and they will be pattern ids 0 - 15.
    int next_perfect_pattern_id = 0;
    // A regular pattern is anything else.
    int next_regular_pattern_id = 16;

    int pattern_id;
    for (int i = 0; i < FACE_ARRANGEMENTS; ++i) {
        if (NOT_SET != face_table[i]) {
            continue;
        }

        unsigned char color_count, max_instances, color_connectedness;
        int val = i;
        unsigned char face_colors[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

        FaceIndexToColors(i, face_colors);
        GetFaceColorCounts(face_colors, &color_count, &max_instances);
        color_connectedness = GetFaceColorConnectedness(face_colors);

        if ((color_count == CUBE_COLORS) && (max_instances == 2) && (color_connectedness == NOTHING_TOUCHING)) {
            pattern_id = next_perfect_pattern_id++;
        }
        else {
            pattern_id = next_regular_pattern_id++;
        }

        int color_swaps[CUBE_COLORS] = { 0, 1, 2, 3, 4, 5 };
        for (int symmetry = 0; symmetry < 8; ++symmetry) {
            for (int c0 = 0; c0 < CUBE_COLORS; ++c0) {
                SWAP(color_swaps[0], color_swaps[c0]);
                for (int c1 = 1; c1 < CUBE_COLORS; ++c1) {
                    SWAP(color_swaps[1], color_swaps[c1]);
                    for (int c2 = 2; c2 < CUBE_COLORS; ++c2) {
                        SWAP(color_swaps[2], color_swaps[c2]);
                        for (int c3 = 3; c3 < CUBE_COLORS; ++c3) {
                            SWAP(color_swaps[3], color_swaps[c3]);
                            for (int c4 = 4; c4 < CUBE_COLORS; ++c4) {
                                SWAP(color_swaps[4], color_swaps[c4]);

                                int idx = 0;
                                for (int surface = 8; surface >= 0; --surface) {
                                    idx = idx * CUBE_COLORS + color_swaps[face_colors[symmetries[symmetry][surface]]];
                                }

                                if (face_table[idx] == NOT_SET) {
                                    face_table[idx] = pattern_id;
                                }

                                SWAP(color_swaps[4], color_swaps[c4]);
                            }
                            SWAP(color_swaps[3], color_swaps[c3]);
                        }
                        SWAP(color_swaps[2], color_swaps[c2]);
                    }
                    SWAP(color_swaps[1], color_swaps[c1]);
                }
                SWAP(color_swaps[0], color_swaps[c0]);
            }
        }
    }
}


// Write face_table[] to a file.
bool WriteFaceTable() {
    FILE* fp;
    errno_t err = fopen_s(&fp, "FaceTable.dat", "wb");
    if ((0 != err) || (NULL == fp)) {
        fprintf(stderr, "Failed to write FaceTable.dat\n");
        return false;
    }

    fwrite(face_table, sizeof(__int16), FACE_ARRANGEMENTS, fp);
    fclose(fp);
    fp = NULL;

    return true;
}


// Read face_table from a file.
bool ReadFaceTable() {
    FILE* fp;
    errno_t err = fopen_s(&fp, "FaceTable.dat", "rb");
    if ((0 != err) || (NULL == fp)) {
        fprintf(stdout, "Building the face table.\n");
        BuildFaceTable();
        WriteFaceTable();
        return true;
    }

    fprintf(stdout, "Reading the face table.\n");
    fread(face_table, sizeof(__int16), FACE_ARRANGEMENTS, fp);
    fclose(fp);
    fp = NULL;

    face_table_filled = true;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Evaluation of the entire cube.
// 
////////////////////////////////////////////////////////////////////////////////////////////////////


// See how connected a cube is.
// See ScrambleSearcher.cpp for the cube layout diagram.
int GetColorConnectedness(unsigned char cube[CUBE_SURFACES])
{
    // Surfaces to compare to see if there are two side-by-side surfaces with the same color.
    static const unsigned char sides[72][2] = {
        {  0,  1 },{  1,  2 },{  3,  4 },{  4,  5 },{  6,  7 },{  7,  8 },{  0,  3 },{  1,  4 },{  2,  5 },{  3,  6 },{  4,  7 },{  5,  8 },
        {  9, 10 },{ 10, 11 },{ 12, 13 },{ 13, 14 },{ 15, 16 },{ 16, 17 },{  9, 12 },{ 10, 13 },{ 11, 14 },{ 12, 15 },{ 13, 16 },{ 14, 17 },
        { 18, 19 },{ 19, 20 },{ 21, 22 },{ 22, 23 },{ 24, 25 },{ 25, 26 },{ 18, 21 },{ 19, 22 },{ 20, 23 },{ 21, 24 },{ 22, 25 },{ 23, 26 },
        { 27, 28 },{ 28, 29 },{ 30, 31 },{ 31, 32 },{ 33, 34 },{ 34, 35 },{ 27, 30 },{ 28, 31 },{ 29, 32 },{ 30, 33 },{ 31, 34 },{ 32, 35 },
        { 36, 37 },{ 37, 38 },{ 39, 40 },{ 40, 41 },{ 42, 43 },{ 43, 44 },{ 36, 39 },{ 37, 40 },{ 38, 41 },{ 39, 42 },{ 40, 43 },{ 41, 44 },
        { 45, 46 },{ 46, 47 },{ 48, 49 },{ 49, 50 },{ 51, 52 },{ 52, 53 },{ 45, 48 },{ 46, 49 },{ 47, 50 },{ 48, 51 },{ 49, 52 },{ 50, 53 }
    };

    // Surfaces to compare to see if there are two surfaces with the same color, touching on the corners.
    static const unsigned char corners[48][2] = {
        {  0,  4 }, {  2,  4 }, {  6,  4 }, {  8,  4 }, {  1,  3 }, {  1,  5 }, {  7,  3 }, {  7,  5 },
        {  9, 13 }, { 11, 13 }, { 15, 13 }, { 17, 13 }, { 10, 12 }, { 10, 14 }, { 16, 12 }, { 16, 14 },
        { 18, 22 }, { 20, 22 }, { 24, 22 }, { 26, 22 }, { 19, 21 }, { 19, 23 }, { 25, 21 }, { 25, 23 },
        { 27, 31 }, { 29, 31 }, { 33, 31 }, { 35, 31 }, { 28, 30 }, { 28, 32 }, { 34, 30 }, { 34, 32 },
        { 36, 40 }, { 38, 40 }, { 42, 40 }, { 44, 40 }, { 37, 39 }, { 37, 41 }, { 43, 39 }, { 43, 41 },
        { 45, 49 }, { 47, 49 }, { 51, 49 }, { 53, 49 }, { 46, 48 }, { 46, 50 }, { 52, 48 }, { 52, 50 }
    };

    // Surfaces to compare to see if there are two surfaces with the same color, touching on the corners across two adjacent faces.
    static const unsigned char fcorners[48][2] = {
        { 19,  6 }, { 19,  8 }, { 21, 11 }, { 21, 17 }, { 23, 27 }, { 23, 33 }, { 25, 36 }, { 25, 38 },
        { 37, 24 }, { 37, 26 }, { 39, 17 }, { 39, 15 }, { 41, 33 }, { 41, 35 }, { 43, 45 }, { 43, 47 },
        { 46, 42 }, { 46, 44 }, { 48, 15 }, { 48,  9 }, { 50, 35 }, { 50, 29 }, { 52,  0 }, { 52,  2 },
        {  1, 51 }, {  1, 53 }, {  3,  9 }, {  3, 11 }, {  5, 29 }, {  5, 27 }, {  7, 18 }, {  7, 20 },
        { 10,  0 }, { 10,  6 }, { 12, 51 }, { 12, 45 }, { 14, 18 }, { 14, 24 }, { 16, 42 }, { 16, 36 },
        { 28,  8 }, { 28,  2 }, { 30, 20 }, { 30, 26 }, { 32, 53 }, { 32, 47 }, { 34, 38 }, { 34, 44 }
    };

    unsigned char colors[CUBE_SURFACES] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    // Fill out the color of each surface for fast color comparison below.
    for (int i = 0; i < CUBE_SURFACES; ++i) {
        colors[i] = cube[i] / 9;
    }

    // Check to see if two of the same color are touching on a side, on the same face
    for (int idx = 0; idx < 72; ++idx) {
        if (colors[sides[idx][0]] == colors[sides[idx][1]]) {
            return SIDES_TOUCHING;
        }
    }

    // Check to see if two of the same color are touching on a corner, on the same face.
    for (int idx = 0; idx < 48; ++idx) {
        if (colors[corners[idx][0]] == colors[corners[idx][1]]) {
            return CORNERS_TOUCHING;
        }
    }

    // Check to see if two of the same color are touching on a corner, on adjacent faces.
    for (int idx = 0; idx < 48; ++idx) {
        if (colors[fcorners[idx][0]] == colors[fcorners[idx][1]]) {
            return ADJACENT_FACES_TOUCHING;
        }
    }

    return NOTHING_TOUCHING;
}
