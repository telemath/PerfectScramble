// ScrambleSearcher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
// This looks for Rubik's cube scrambles that:
//   * Have all 6 colors on every face.
//   * Have no more than 2 squares of any color on a face.
//   * Have no two squares of the same color touching on a side or on a corner.
//  Optionally, it looks for:
//   * No two squares of the same color touching on a corner wehre two faces meet.
//   * A different pattern on every face.
//
//  The cube is laid out using 54 "surfaces":
// 
//               Back Face
//               +--+--+--+
//               | 0| 1| 2|
//               +--+--+--+
//               | 3| 4| 5|
//               +--+--+--+
//               | 6| 7| 8|
//               +--+--+--+
//   Left Face    Up Face    Right Face
//   +--+--+--+  +--+--+--+  +--+--+--+
//   | 9|10|11|  |18|19|20|  |27|28|29|
//   +--+--+--+  +--+--+--+  +--+--+--+
//   |12|13|14|  |21|22|23|  |30|31|32|
//   +--+--+--+  +--+--+--+  +--+--+--+
//   |15|16|17|  |24|25|26|  |33|34|35|
//   +--+--+--+  +--+--+--+  +--+--+--+
//               Front Face
//               +--+--+--+
//               |36|37|38|
//               +--+--+--+
//               |39|40|41|
//               +--+--+--+
//               |42|43|44|
//               +--+--+--+
//               Down Face
//               +--+--+--+
//               |45|46|47|
//               +--+--+--+
//               |48|49|50|
//               +--+--+--+
//               |51|52|53|
//               +--+--+--+
//
// This defines both the positions and the pieces. E.g. there is corner piece that has surfaces 6, 11, and 18.
// Moving the corner piece will move all three surfaces. When the cube is in its solved state, surface 6 will be in position 6, etc.

#include <stdlib.h>
#include <stdio.h>
#include <ostream>
#include "ScrambleEvaluation.h"

#pragma region Utilities
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Utilities
//
////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr auto CUBE_COLORS_SQ = CUBE_COLORS * CUBE_COLORS;

// The color of each surface in a cube. colors[x] seems marginally faster than x / 9, though I haven't done any formal timing tests.
const unsigned char colors[CUBE_SURFACES] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                                              3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5 };


// Get the color of a surface.
inline unsigned char ColorOf(unsigned char x)
{
    unsigned char color = colors[x];
    return color;
}

// Compare the first [count] entries in two arrays of Face Ids.
int compareFaceIds(unsigned int* a, unsigned int* b, int count)
{
    for (int i = 0; i < count; ++i) {
        if (a[i] < b[i]) {
            return -1;
        }
        if (a[i] > b[i]) {
            return 1;
        }
    }

    return 0;
}

#pragma endregion Utilities

#pragma region Corners
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Corner Arrangements
//
////////////////////////////////////////////////////////////////////////////////////////////////////

//
// Corner Data
// 

// The surfaces for each corner piece.
const unsigned char corners[CUBE_CORNERS][3] = { { 18, 11,  6 }, { 20,  8, 27 }, { 24, 36, 17 }, { 26, 33, 38 },
                                                 { 45, 15, 42 }, { 47, 44, 35 }, { 51,  0,  9 }, { 53, 29,  2 } };

// Positions to check to ensure that no three corners on the same face have the same color.
const unsigned char corner_count_checks[24][3] = {
            { 18, 20, 24 },                                                                 // Index   0   - Requires corner 2.
            { 18, 20, 26 }, { 18, 24, 26 }, { 20, 24, 26 },                                 // Index 1-3   - Requires corner 3.
            { 11, 15, 17 }, { 36, 38, 42 },                                                 // Index 4-5   - Requires corner 4.
            { 27, 33, 35 }, { 36, 38, 44 }, { 36, 42, 44 }, { 38, 42, 44 },                 // Index 6-9   - Requires corner 5.
            {  0,  6,  8 }, { 45, 47, 51 }, {  9, 11, 15 }, {  9, 11, 17 }, {  9, 15, 17 }, // Index 10-14 - Requires corner 6.
            {  0,  2,  6 }, {  0,  2,  8 }, {  2,  6,  8 }, { 27, 29, 33 }, { 27, 29, 35 }, // Index 15-23 - Requires corner 7.
            { 29, 33, 35 }, { 45, 47, 53 }, { 45, 51, 53 }, { 47, 51, 53 } };

// Once corner piece N is placed, apply corner count checks from corner_count_checks_start[N] through corner_count_checks_end[N].
const unsigned char corner_count_checks_start[CUBE_CORNERS] = {  99, 99, 0, 1, 4, 6, 10, 15};
const unsigned char corner_count_checks_end  [CUBE_CORNERS] = {  98, 98, 0, 3, 5, 9, 14, 23};

//
// Cached data - All the acceptable ways to arrange the corner pieces.
//

typedef struct {
    unsigned char arrangement[CUBE_SURFACES];  // The positions of the corner pieces. To be OR'ed together with edge arrangements.
    unsigned int faceIds[CUBE_FACES];          // The corners' contribution to each face arrangement. Includes the center piece. To be added to the edges' contribution.
    unsigned int nextIndex[CUBE_FACES];        // The index of the first entry that contains a different value.
} CornerArrangement;

// Hard-coded because I worked it out once and now I can be lazy with array sizes.
const int EP_CORNER_ARRANGEMENT_COUNT = 375336;
const int OP_CORNER_ARRANGEMENT_COUNT = 375304;

CornerArrangement ep_corner_arrangements[EP_CORNER_ARRANGEMENT_COUNT];
CornerArrangement op_corner_arrangements[OP_CORNER_ARRANGEMENT_COUNT];

// Used when filling ep/op_corner_arrangements.
int ep_corner_arrangement_count = 0;
int op_corner_arrangement_count = 0;


bool ReadCornerArrangements()
{
    FILE* fp = NULL;
    errno_t result = fopen_s(&fp, "Corners.dat", "rb");
    if ((result != 0) || (fp == NULL)) {
        return false;
    }

    int records_read = (int)fread(ep_corner_arrangements, sizeof(CornerArrangement), EP_CORNER_ARRANGEMENT_COUNT, fp);
    if (records_read != EP_CORNER_ARRANGEMENT_COUNT) {
        fprintf(stderr, "Reading ep_corner_arrangements - expected to read %d, but read %d.\n", EP_CORNER_ARRANGEMENT_COUNT, records_read);
        fclose(fp);
        return false;
    }

    records_read = (int)fread(op_corner_arrangements, sizeof(CornerArrangement), OP_CORNER_ARRANGEMENT_COUNT, fp);
    if (records_read != OP_CORNER_ARRANGEMENT_COUNT) {
        fprintf(stderr, "Reading op_corner_arrangements - expected to read %d, but read %d.\n", OP_CORNER_ARRANGEMENT_COUNT, records_read);
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}


bool WriteCornerArrangements()
{
    FILE* fp = NULL;
    errno_t result = fopen_s(&fp, "Corners.dat", "wb");
    if ((result != 0) || (fp == NULL)) {
        if (fp == NULL) fprintf(stderr, "WriteCornerArrangements() failed to open file - errno=%d, fp=%p\n", result, fp);
        return false;
    }

    int records_written = (int)fwrite(ep_corner_arrangements, sizeof(CornerArrangement), EP_CORNER_ARRANGEMENT_COUNT, fp);
    if (records_written != EP_CORNER_ARRANGEMENT_COUNT) {
        fprintf(stderr, "Writing ep_corner_arrangements - expected to write %d, but wrote %d.\n", EP_CORNER_ARRANGEMENT_COUNT, records_written);
        fclose(fp);
        return false;
    }

    records_written = (int)fwrite(op_corner_arrangements, sizeof(CornerArrangement), OP_CORNER_ARRANGEMENT_COUNT, fp);
    if (records_written != OP_CORNER_ARRANGEMENT_COUNT) {
        fprintf(stderr, "Writing op_corner_arrangements - expected to write %d, but wrote %d.\n", OP_CORNER_ARRANGEMENT_COUNT, records_written);
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}


void StoreCornerArrangement(unsigned char cube[CUBE_SURFACES], CornerArrangement* corner_arrangements, int corner_arrangement_count)
{
    // Calculate the corners' contributions to the face arrangement
    unsigned int face_ids[CUBE_FACES] = { 0, 0, 0, 0, 0, 0 };

    for (int i = 0; i < CUBE_FACES; ++i) {
        int start = i * 9;
        face_ids[i] = ((((cube[start] / 9) * CUBE_COLORS_SQ + (cube[start + 2] / 9)) * CUBE_COLORS_SQ + (cube[start + 4] / 9)) * CUBE_COLORS_SQ + (cube[start + 6] / 9)) * CUBE_COLORS_SQ + (cube[start + 8] / 9);
    }

    // Find the position to insert the cube arrangmeent at.
    int insert_at = 0;
    int lower = 0;
    int upper = corner_arrangement_count - 1;

    if (corner_arrangement_count == 0) {
        insert_at = 0;
    }
    else if (compareFaceIds(face_ids, corner_arrangements[lower].faceIds, CUBE_FACES) < 0) {
        insert_at = 0;
    }
    else if (compareFaceIds(face_ids, corner_arrangements[upper].faceIds, CUBE_FACES) > 0) {
        insert_at = upper + 1;
    }
    else {
        while (lower + 1 < upper) {
            int mid = (upper + lower) / 2;
            int comp = compareFaceIds(face_ids, corner_arrangements[mid].faceIds, CUBE_FACES);
            if (comp < 0) {
                upper = mid;
            }
            else {
                lower = mid;
            }
        };
        insert_at = upper;
    }

    // Move stuff to make room for the new arrangement.
    for (int i = corner_arrangement_count - 1; i >= insert_at; --i) {
        memcpy(&(corner_arrangements[i + 1]), &(corner_arrangements[i]), sizeof(CornerArrangement));
    }

    // Insert the new arrangement.
    memcpy(corner_arrangements[insert_at].arrangement, cube, CUBE_SURFACES * sizeof(unsigned char));
    memcpy(corner_arrangements[insert_at].faceIds, face_ids, CUBE_FACES * sizeof(unsigned int));
}


void PlaceLastCornerPiece(unsigned char corner_num, unsigned char pieces[CUBE_CORNERS], unsigned char cube[CUBE_SURFACES], unsigned char swap_parity, unsigned char rotation_parity)
{
    // If placing the last piece, the piece and rotation are determined by the previous selections
    unsigned char ori = (3 - rotation_parity) % 3;

    // Place the piece.
    cube[corners[corner_num][0]] = corners[pieces[corner_num]][(0 + ori) % 3];
    cube[corners[corner_num][1]] = corners[pieces[corner_num]][(1 + ori) % 3];
    cube[corners[corner_num][2]] = corners[pieces[corner_num]][(2 + ori) % 3];

    // Check to make sure that no corner surface has the same color as the center (no CORNERS_TOUCHING).
    if ((ColorOf(cube[corners[corner_num][0]]) == ColorOf(corners[corner_num][0])) ||
        (ColorOf(cube[corners[corner_num][1]]) == ColorOf(corners[corner_num][1])) ||
        (ColorOf(cube[corners[corner_num][2]]) == ColorOf(corners[corner_num][2]))) {
        return;
    }

    // Check to make sure that we don't have 3 corners of the same color on a single face.
    bool color_count_passed = true;
    for (int i = corner_count_checks_start[corner_num]; i <= corner_count_checks_end[corner_num]; ++i) {
        color_count_passed &= (ColorOf(cube[corner_count_checks[i][0]]) != ColorOf(cube[corner_count_checks[i][1]])) ||
            (ColorOf(cube[corner_count_checks[i][0]]) != ColorOf(cube[corner_count_checks[i][2]]));
    }

    if (!color_count_passed) {
        return;
    }

    if (swap_parity == 0) {
        StoreCornerArrangement(cube, ep_corner_arrangements, ep_corner_arrangement_count);
        ++ep_corner_arrangement_count;
    }
    else {
        StoreCornerArrangement(cube, op_corner_arrangements, op_corner_arrangement_count);
        ++op_corner_arrangement_count;
    }

    // Show progress.
    static int count = 0;
    if ((++count % 7506) == 0)
    {
        int pct = count / 7506;
        printf("%i%% done. ", pct);
        if (((pct % 7) == 0) || (pct == 100)) {
            printf("\n");
        }
    }

    return;
}


void PlaceCornerPiece(unsigned char corner_num, unsigned char pieces[CUBE_CORNERS], unsigned char cube[CUBE_SURFACES], unsigned char swap_parity, unsigned char rotation_parity)
{
    if (corner_num == 7) {
        PlaceLastCornerPiece(corner_num, pieces, cube, swap_parity, rotation_parity);
    }

    // Select corner piece.
    for (int pos = corner_num; pos < CUBE_CORNERS; ++pos) {
        unsigned char temp;

        if (pos != corner_num) {
            // Swap the piece into place in the pieces array.
            temp = pieces[corner_num];
            pieces[corner_num] = pieces[pos];
            pieces[pos] = temp;

            // Track swap parity.
            swap_parity ^= 1;
        }

        // Select orientation.
        for (int ori = 0; ori < 3; ++ori) {
            // Track rotation parity.
            rotation_parity = (rotation_parity + ori) % 3;

            // Place the piece.
            cube[corners[corner_num][0]] = corners[pieces[corner_num]][(0 + ori) % 3];
            cube[corners[corner_num][1]] = corners[pieces[corner_num]][(1 + ori) % 3];
            cube[corners[corner_num][2]] = corners[pieces[corner_num]][(2 + ori) % 3];

            // Check to make sure that no corner surface has the same color as the center (no CORNERS_TOUCHING).
            if ((ColorOf(cube[corners[corner_num][0]]) == ColorOf(corners[corner_num][0])) ||
                (ColorOf(cube[corners[corner_num][1]]) == ColorOf(corners[corner_num][1])) ||
                (ColorOf(cube[corners[corner_num][2]]) == ColorOf(corners[corner_num][2]))) {
                // Undo rotation parity.
                rotation_parity = (rotation_parity + 3 - ori) % 3;
                continue;
            }

            // Check to make sure that we don't have 3 corner surfaces of the same color on a single face.
            bool three_same_color = false;
            for (int i = corner_count_checks_start[corner_num]; (i <= corner_count_checks_end[corner_num]) && !three_same_color; ++i) {
                three_same_color = (ColorOf(cube[corner_count_checks[i][0]]) == ColorOf(cube[corner_count_checks[i][1]])) &&
                    (ColorOf(cube[corner_count_checks[i][0]]) == ColorOf(cube[corner_count_checks[i][2]]));
            }

            if (three_same_color) {
                // Undo rotation parity.
                rotation_parity = (rotation_parity + 3 - ori) % 3;
                continue;
            }

            // Recursive call to place the next corner piece.
            PlaceCornerPiece(corner_num + 1, pieces, cube, swap_parity, rotation_parity);

            // Undo rotation parity.
            rotation_parity = (rotation_parity + 3 - ori) % 3;
        }

        // Undo piece swap and swap parity.
        if (pos != corner_num) {
            temp = pieces[corner_num];
            pieces[corner_num] = pieces[pos];
            pieces[pos] = temp;

            swap_parity ^= 1;
        }
    }
}


void FillCornerIndexes(CornerArrangement* corner_arrangements, int corner_arrangement_count) {
    for (int arrangement = 0; arrangement < corner_arrangement_count; ++arrangement) {
        for (int index = 0; index < CUBE_FACES; ++index) {
            if ((arrangement > 0) && (compareFaceIds(corner_arrangements[arrangement].faceIds, corner_arrangements[arrangement - 1].faceIds, index + 1) == 0)) {
                corner_arrangements[arrangement].nextIndex[index] = corner_arrangements[arrangement - 1].nextIndex[index];
            }
            else {
                int next_arrangement;
                for (next_arrangement = arrangement + 1;
                    (next_arrangement < corner_arrangement_count) &&
                    (compareFaceIds(corner_arrangements[arrangement].faceIds, corner_arrangements[next_arrangement].faceIds, index + 1) == 0);
                    ++next_arrangement);
                if (next_arrangement >= corner_arrangement_count) {
                    corner_arrangements[arrangement].nextIndex[index] = -1;
                }
                else {
                    corner_arrangements[arrangement].nextIndex[index] = next_arrangement;
                }
            }
        }
    }
}


void CreateCornerArrangements()
{
    // The positions of the corner pieces. pieces[3] = 5 means that corner piece 5 is in corner piece 3's position.
    unsigned char pieces[CUBE_CORNERS] = { 0, 1, 2, 3, 4, 5, 6, 7 };

    // The cube. Corners will be overwritten in PlaceCornerPiece().
    // Set corner surfaces to 99 for safety checks.
    // Set edge surfaces as 0 so they can be OR'ed in later.
    // Set centers to actual values so they can be compared to the corner colors.
    unsigned char cube[CUBE_SURFACES] = { 99, 0, 99, 0,  4, 0, 99, 0, 99, 99, 0, 99, 0, 13, 0, 99, 0, 99, 99, 0, 99, 0, 22, 0, 99, 0, 99,
                                          99, 0, 99, 0, 31, 0, 99, 0, 99, 99, 0, 99, 0, 40, 0, 99, 0, 99, 99, 0, 99, 0, 49, 0, 99, 0, 99 };

    PlaceCornerPiece(0, pieces, cube, 0, 0);

    FillCornerIndexes(ep_corner_arrangements, ep_corner_arrangement_count);
    FillCornerIndexes(op_corner_arrangements, op_corner_arrangement_count);
}


int GetCornerArrangementsIndex(int index, CornerArrangement* corner_arrangements, unsigned int * face_ids, int face_id_count, int max_index)
{
    if ((index > max_index) || (index == -1)) {
        return -1;
    }

    for (int face_num = 0; face_num < face_id_count; ) {
        int face_idx = corner_arrangements[index].faceIds[face_num] + face_ids[face_num];
        __int16 face_id = face_table[face_idx];
        bool valid = face_id < 16;

        if (valid) {
            ++face_num;
        }
        else {
            index = corner_arrangements[index].nextIndex[face_num];
            if (index == -1) {
                break;
            }
            face_num = 0;
        }
    }

    return index;
}


#pragma endregion Corners


#pragma region Edges
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Edge Arrangements
//
////////////////////////////////////////////////////////////////////////////////////////////////////

// The surfaces for each edge piece.
const unsigned char edges[CUBE_EDGES][2] = { { 52,  1 }, {  3, 10 }, {  5, 28 }, { 19,  7 }, { 48, 12 }, { 21, 14 },
                                           { 39, 16 }, { 23, 30 }, { 25, 37 }, { 50, 32 }, { 41, 34 }, { 46, 43 }};

// Positions to check to ensure that no edge surfaces, touching at a diagonal, have the same color.
const unsigned char edge_diagonal_checks[24][2] = { {  1,  3 }, {  1,  5 }, {  3,  7 }, {  5,  7 }, { 10, 12 }, { 48, 52 },
                                                    { 10, 14 }, { 19, 21 }, { 12, 16 }, { 14, 16 }, { 19, 23 }, { 28, 30 },
                                                    { 21, 25 }, { 23, 25 }, { 37, 39 }, { 28, 32 }, { 50, 52 }, { 30, 34 },
                                                    { 32, 34 }, { 37, 41 }, { 39, 43 }, { 41, 43 }, { 46, 48 }, { 46, 50 }};

// Once edge piece N is placed, apply edge diagonal checks from edge_diagonal_checks_start[N] through edge_diagonal_checks_ends[N].
const unsigned char edge_diagonal_checks_start[CUBE_EDGES] = { 99, 0, 1, 2, 4, 6, 8, 10, 12, 15, 17, 20};
const unsigned char edge_diagonal_checks_end  [CUBE_EDGES] = { 98, 0, 1, 3, 5, 7, 9, 11, 14, 16, 19, 23};

// Once edge piece N is places, apply face id checks from edge_face_id_checks_start[N] through edge_face_id_checks_end[N].
const int edge_face_id_checks_start[CUBE_EDGES] = { -1, -1, -1, 0, -1, -1, 1, -1, 2, -1, 3, 4 };
const int edge_face_id_checks_end  [CUBE_EDGES] = { -2, -2, -2, 0, -2, -1, 1, -2, 2, -2, 3, 5 };

unsigned long int edge_arrangements = 0;
unsigned long int odd_edge_arrangements = 0;
unsigned long int even_edge_arrangements = 0;
char edge_ids[13] = "0123456789AB";
char edge_progress[25] = "                        ";


void RecordSolution(unsigned int face_ids[CUBE_FACES], unsigned char cube[CUBE_SURFACES], CornerArrangement* corner_arrangements, int corner_arrangements_index)
{
    static long int solution_counts[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    // Get the face ids
    int unique_patterns = 6;
    __int16 solution_face_ids[CUBE_FACES] = { 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < CUBE_FACES; ++i) {
        solution_face_ids[i] = face_table[face_ids[i] + corner_arrangements[corner_arrangements_index].faceIds[i]];

        for (int j = 0; j < i; ++j) {
            if (solution_face_ids[j] == solution_face_ids[i]) {
                --unique_patterns; // This face has the same pattern as a previous face.
                break;
            }
        }
    }

    // Assemble the final cube.
    unsigned char solution_cube[CUBE_SURFACES] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < CUBE_SURFACES; ++i) {
        solution_cube[i] = cube[i] | corner_arrangements[corner_arrangements_index].arrangement[i];
    }

    // Get the overall color connectedness.
    int connectedness = GetColorConnectedness(solution_cube);

    if (connectedness < ADJACENT_FACES_TOUCHING) {
        fprintf(stderr, "Corners or sides touching in a solution cube. This should not have reached a solution.\n");
        return;
    }

    // Get the filename to save this result to.
    char filename[100];
    sprintf_s(filename, "Solutions_%i_patterns%s.txt", unique_patterns, ((connectedness == ADJACENT_FACES_TOUCHING) ? "" : "_Perfect"));

    // Write the solution to the file.
    FILE* fp = NULL;
    errno_t result = fopen_s(&fp, filename, "a");
    if ((result != 0) || (fp == NULL)) {
        fprintf(stderr, "Unable to open solution file: %s\n", filename);
        return;
    }

    fprintf(fp, "%i", solution_cube[0]);
    for (int i = 1; i < CUBE_SURFACES; ++i) {
        fprintf(fp, ",%i", solution_cube[i]);
    }
    fprintf(fp, "\n");
    fclose(fp);
    fp = NULL;

    static int total_solutions = 0;
    ++total_solutions;
    ++solution_counts[unique_patterns - 1 + ((connectedness == ADJACENT_FACES_TOUCHING) ? 0 : 6)];
    if (((total_solutions % 100) == 0) || (connectedness == NOTHING_TOUCHING)) {
        printf("%s   solutions: ", edge_progress);
        for (int i = 0; i < 12; ++i) {
            printf(" %i", solution_counts[i]);
        }
        printf("\n");
    }
}


void PlaceLastEdgePiece(unsigned char edge_num, unsigned char pieces[CUBE_EDGES], unsigned char cube[CUBE_SURFACES], unsigned char swap_parity, unsigned char flip_parity, unsigned int face_ids[CUBE_FACES], int corner_arrangements_index)
{
    // If placing the last piece, the piece and rotation are determined by the previous selections
    unsigned char ori = flip_parity;

    cube[edges[edge_num][0]] = edges[pieces[edge_num]][ori];
    cube[edges[edge_num][1]] = edges[pieces[edge_num]][1 ^ ori];

    edge_progress[2 * edge_num] = edge_ids[pieces[edge_num]];
    edge_progress[2 * edge_num + 1] = ori ? '-' : '_';

    // Check to make sure that no edge surface has the same color as the center (no SIDES_TOUCHING).
    if ((ColorOf(cube[edges[edge_num][0]]) == ColorOf(edges[edge_num][0])) ||
        (ColorOf(cube[edges[edge_num][1]]) == ColorOf(edges[edge_num][1]))) {
        edge_progress[2 * edge_num] = ' ';
        edge_progress[2 * edge_num + 1] = ' ';
        return;
    }

    // Check to make sure that two edge surfaces, touching at a diagonal, don't have the same color.
    for (int i = edge_diagonal_checks_start[edge_num]; (i <= edge_diagonal_checks_end[edge_num]); ++i) {
        if (ColorOf(cube[edge_diagonal_checks[i][0]]) == ColorOf(cube[edge_diagonal_checks[i][1]])) {
            edge_progress[2 * edge_num] = ' ';
            edge_progress[2 * edge_num + 1] = ' ';
            return;
        }
    }

    // Fill out the edges' contribution to each face ids.
    for (int face_index = edge_face_id_checks_start[edge_num]; face_index <= edge_face_id_checks_end[edge_num]; ++face_index) {
        int start = face_index * 9;
        face_ids[face_index] = ((((cube[start + 1] / 9) * CUBE_COLORS_SQ + (cube[start + 3] / 9)) * CUBE_COLORS_SQ + (cube[start + 5] / 9)) * CUBE_COLORS_SQ + (cube[start + 7] / 9)) * CUBE_COLORS;
    }

    ++edge_arrangements;
    if (swap_parity == 0)
        ++even_edge_arrangements;
    else
        ++odd_edge_arrangements;

    CornerArrangement* arrangements = (swap_parity == 0) ? ep_corner_arrangements : op_corner_arrangements;
    int max_index = ((swap_parity == 0) ? EP_CORNER_ARRANGEMENT_COUNT : OP_CORNER_ARRANGEMENT_COUNT) - 1;
    corner_arrangements_index = GetCornerArrangementsIndex(corner_arrangements_index, arrangements, face_ids, edge_face_id_checks_end[edge_num] + 1, max_index);

    while (corner_arrangements_index != -1) {
        RecordSolution(face_ids, cube, arrangements, corner_arrangements_index);
        if (corner_arrangements_index >= max_index) {
            break;
        }
        corner_arrangements_index = GetCornerArrangementsIndex(corner_arrangements_index + 1, arrangements, face_ids, edge_face_id_checks_end[edge_num] + 1, max_index);
    }

    edge_progress[2 * edge_num] = ' ';
    edge_progress[2 * edge_num + 1] = ' ';
}


void PlaceEdgePiece(unsigned char edge_num, unsigned char pieces[CUBE_EDGES], unsigned char cube[CUBE_SURFACES], unsigned char swap_parity, unsigned char flip_parity, unsigned int face_ids[CUBE_FACES], int ep_corner_arrangements_index, int op_corner_arrangements_index)
{
    if (edge_num == 11) {
        PlaceLastEdgePiece(edge_num, pieces, cube, swap_parity, flip_parity, face_ids, (swap_parity == 0) ? ep_corner_arrangements_index : op_corner_arrangements_index);
    }

    // Select corner piece.
    for (int pos = edge_num; pos < CUBE_EDGES; ++pos) {
        unsigned char temp;

        if (pos != edge_num) {
            // Swap the piece into place in the pieces array.
            temp = pieces[edge_num];
            pieces[edge_num] = pieces[pos];
            pieces[pos] = temp;

            // Track swap parity.
            swap_parity ^= 1;
        }

        // Select orientation.
        for (int ori = 0; ori < 2; ++ori) {
            // Track rotation parity.
            flip_parity ^= ori;

            // Place the piece.
            cube[edges[edge_num][0]] = edges[pieces[edge_num]][ori];
            cube[edges[edge_num][1]] = edges[pieces[edge_num]][1 ^ ori];

            // Check to make sure that no edge surface has the same color as the center (no SIDES_TOUCHING).
            if ((ColorOf(cube[edges[edge_num][0]]) == ColorOf(edges[edge_num][0])) ||
                (ColorOf(cube[edges[edge_num][1]]) == ColorOf(edges[edge_num][1]))) {
                // Undo flip parity.
                flip_parity ^= ori;
                continue;
            }

            // Check to make sure that two edge surfaces, touching at a diagonal, don't have the same color.
            bool diagonals_touching = false;
            for (int i = edge_diagonal_checks_start[edge_num]; (i <= edge_diagonal_checks_end[edge_num]) && !diagonals_touching; ++i) {
                diagonals_touching = ColorOf(cube[edge_diagonal_checks[i][0]]) == ColorOf(cube[edge_diagonal_checks[i][1]]);
            }

            if (diagonals_touching) {
                // Undo rotation parity.
                flip_parity ^= ori;
                continue;
            }

            int next_ep_corner_arrangements_index = ep_corner_arrangements_index;
            int next_op_corner_arrangements_index = op_corner_arrangements_index;
            bool indices_ok = true;

            if (edge_face_id_checks_start[edge_num] >= 0) {
                int face_id_count = edge_face_id_checks_end[edge_num] + 1;
                // Fill out the edges' contribution to each face ids.
                for (int face_index = edge_face_id_checks_start[edge_num]; face_index < face_id_count; ++face_index) {
                    int start = face_index * 9;
                    face_ids[face_index] = ((((cube[start + 1] / 9) * CUBE_COLORS_SQ + (cube[start + 3] / 9)) * CUBE_COLORS_SQ + (cube[start + 5] / 9)) * CUBE_COLORS_SQ + (cube[start + 7] / 9)) * CUBE_COLORS;
                }

                next_ep_corner_arrangements_index = GetCornerArrangementsIndex(ep_corner_arrangements_index, ep_corner_arrangements, face_ids, face_id_count, EP_CORNER_ARRANGEMENT_COUNT - 1);
                next_op_corner_arrangements_index = GetCornerArrangementsIndex(op_corner_arrangements_index, op_corner_arrangements, face_ids, face_id_count, OP_CORNER_ARRANGEMENT_COUNT - 1);
                indices_ok = (next_ep_corner_arrangements_index != -1) || (next_op_corner_arrangements_index != -1);
            }

            if (indices_ok) {
                edge_progress[2 * edge_num] = edge_ids[pieces[edge_num]];
                edge_progress[2 * edge_num + 1] = ori ? '-' : '_';

                // Recursive call to place the next edge piece.
                PlaceEdgePiece(edge_num + 1, pieces, cube, swap_parity, flip_parity, face_ids, next_ep_corner_arrangements_index, next_op_corner_arrangements_index);

                edge_progress[2 * edge_num] = ' ';
                edge_progress[2 * edge_num + 1] = ' ';
            }

            // Undo rotation parity.
            flip_parity ^= ori;
        }

        // Undo piece swap and swap parity.
        if (pos != edge_num) {
            temp = pieces[edge_num];
            pieces[edge_num] = pieces[pos];
            pieces[pos] = temp;

            swap_parity ^= 1;
        }
    }
}


void TryEdgeArrangements()
{
    // The position of the edge pieces. pieces[3] = 5 meains that edge piece 5 is in edge piece 3's position.
    unsigned char pieces[CUBE_EDGES] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

    // The cube. Edges will be overwritten in PlaceEdgePiece().
    // Set edge surfaces to 99 for safety checks.
    // Set corner surfaces as 0 so they can be OR'ed in later.
    // Set centers to actual values so they can be compared to the corner colors.
    unsigned char cube[CUBE_SURFACES] = { 0, 99, 0, 99,  4, 99, 0, 99, 0, 0, 99, 0, 99, 13, 99, 0, 99, 0, 0, 99, 0, 99, 22, 99, 0, 99, 0,
                                          0, 99, 0, 99, 31, 99, 0, 99, 0, 0, 99, 0, 99, 40, 99, 0, 99, 0, 0, 99, 0, 99, 49, 99, 0, 99, 0 };

    int corner_arrangements_index = 0;
    unsigned int face_ids[CUBE_FACES] = { 0, 0, 0, 0, 0, 0 };

    PlaceEdgePiece(0,        // Start with edge number 0.
                   pieces,   // The pieces and positions.
                   cube,     // The resulting cube.
                   0,        // Start with even swap parity, 0.
                   0,        // Start with even flip parity, 0.
                   face_ids, // Face ids.
                   0,        // Start with 0 index into even parity corner arrangements.
                   0);       // Start with 0 index into odd parity corner arrangements.
}

#pragma endregion edges

int main(int argc, char* argv[])
{
    printf("Building face table.\n");
    BuildFaceTable();

    if (!ReadCornerArrangements()) {
        printf("Creating corner arrangements.\n");
        CreateCornerArrangements();
        printf("Created %i even-parity corner arrangements.\n", ep_corner_arrangement_count);
        printf("Created %i  odd-parity corner arrangements.\n", op_corner_arrangement_count);

        if (!WriteCornerArrangements()) {
            fprintf(stderr, "Failed to write corner arrangements to file.\n");
            exit(1);
        }
    }

    printf("Trying edge arrangements\n");
    TryEdgeArrangements();
    printf("%i edge arrangements.\n", edge_arrangements);
    printf("%i even edge arrangements.\n", even_edge_arrangements);
    printf("%i odd edge arrangements.\n", odd_edge_arrangements);
}
