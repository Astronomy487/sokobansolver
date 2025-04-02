#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>

char* FILENAME;

#define MAX_SOK_ROW_LENGTH 512

#define EMPTY (0)
#define GOAL (1)
#define PLAYER (2) // superposition player
#define ORIGINAL_PLAYER (16) //non-superposition player, added during spread
#define BOX (4)
#define WALL (8)

#define TIMEOUT_TIME 600

#define STATES_HASH_TABLE_SIZE 2048

int WIDTH;
int HEIGHT;
int LEFT;
int UP;
int RIGHT;
int DOWN;
int DIRECTION[4];
char* LURD = "LURD";
int N_BOXES;
int N_GOALS;
int MAX_POSSIBLE_FUTURES;
int* GOAL_POSITIONS;
int* BOX_POSITIONS; //changes with each run of heuristic
int** WALKING_DISTANCE_MATRIX; //could make this char ?

struct game_state_node {
	char* level;
	long H;
	int g_score; //length of cheapest known path back to start node
	int h_score; //heuristic, purely a function of level state
	//f_score := g_score + h_score, always always always
	struct game_state_node* previous; //starts as null; is the IDEAL predecessor node
	struct game_state_node** futures; //starts as null; gets populated once we remove it from frontier
	struct game_state_node* next_in_hash_table; //just used to traverse the hash table
};

#define MAX_FRONTIER_SIZE 100000
//TODO ^^^ REPLACE THAT WITH SOME HASH TABLE SCHEME!!! faster lookup will be important one day
struct game_state_node* GAME_STATES_HASH_TABLE[STATES_HASH_TABLE_SIZE];

long state_hash(char* level) { //assumes player spread, assumes particular stage. gives a unique (?) number for every state
	//given a particular stage, a hash for a current state need only be a function of
	// (1) where each of the boxes are
	// (2) the first player location (all contiguous region is also player)
	long val = 0;
	int i;
	for (i = 0; i < WIDTH*HEIGHT; i++) if (level[i] & BOX) val += (i*i*i);
	for (i = 0; true; i++) if (level[i] & PLAYER) return val + i;
}

int heuristic(char* level) {
	/*
	
	I envision a better heuristic algo
	find the location of every box and every goal
	find their distance matrix (really just using WALKING_DISTANCE_MATRIX)
	find minimum-weight matching (graph theory!)
	
	*/
	
	//fill BOX_POSITIONS values
	int i;
	int b = 0;
	for (i = 0; b < N_BOXES; i++) if (level[i] & BOX) BOX_POSITIONS[b++] = i;
	
	//for now, just pair every box to its closest goal, and every goal to its closest box
	//good enough
	//https://pub.ista.ac.at/~vnk/papers/BLOSSOM5.html
	
	int cost = 0;
	for (i = 0; i < N_BOXES; i++) {
		int closest_goal_distance = INT_MAX;
		for (b = 0; b < N_GOALS; b++) {
			int distance = WALKING_DISTANCE_MATRIX[BOX_POSITIONS[i]][GOAL_POSITIONS[b]];
			if (distance < closest_goal_distance) closest_goal_distance = distance;
		}
		cost += closest_goal_distance;
	}
	for (i = 0; i < N_GOALS; i++) {
		int closest_box_distance = INT_MAX;
		for (b = 0; b < N_BOXES; b++) {
			int distance = WALKING_DISTANCE_MATRIX[GOAL_POSITIONS[i]][BOX_POSITIONS[b]];
			if (distance < closest_box_distance) closest_box_distance = distance;
		}
		cost += closest_box_distance;
	}
	return cost / 2;
}

struct game_state_node* make_game_state_node(char* level) {
	long hash = state_hash(level);
	int bucket = hash % STATES_HASH_TABLE_SIZE;
	int i;
	//if another game_state_node already has this hash, free level and return that
	if (GAME_STATES_HASH_TABLE[bucket]) {
		struct game_state_node* node = GAME_STATES_HASH_TABLE[bucket];
		while (node != NULL && node->H != hash) {
			node = node->next_in_hash_table;
		}
		if (node) if (node->H == hash) {
			free(level);
			return node;
		}
	}
	
	//otherwise, make a new node
	struct game_state_node* node = (struct game_state_node*) malloc(sizeof(struct game_state_node));
	node->level = level;
	node->previous = NULL;
	node->H = hash;
	node->g_score = INT_MAX; 
	node->h_score = heuristic(level);
	node->futures = NULL;
	node->next_in_hash_table = GAME_STATES_HASH_TABLE[bucket];
	GAME_STATES_HASH_TABLE[bucket] = node;
	return node;
}

int find_original_player(char* level) {
	int i;
	for (i = 0; i < WIDTH*HEIGHT; i++) if (level[i] & ORIGINAL_PLAYER) return i;
	printf("Couldn't find ORIGINAL_PLAYER in level array\n");
	exit(EXIT_FAILURE);
}
int find_location_of_box_that_moved(char* original, char* new) {
	int i;
	for (i = 0; i < WIDTH*HEIGHT; i++) if (original[i] & BOX) if (!(new[i] & BOX)) return i;
	printf("Couldn't find a box that moved in this state transition\n");
	exit(EXIT_FAILURE);
}

FILE* get_fptr() {
	FILE* fptr = fopen(FILENAME, "r");
	if (fptr == NULL) {
		printf("Couldn't read file \"%s\"\n", FILENAME);
		exit(EXIT_FAILURE);
	}
	return fptr;
}

char* copy_level(char* level) {
	char* new_level = (char*) malloc(sizeof(char) * WIDTH * HEIGHT);
	int i;
	for (i = 0; i < WIDTH*HEIGHT; i++) new_level[i] = level[i];
	return new_level;
}

bool level_is_already_dud(char* level) { //look for Horrible Catastrophic Things That Can't Be Fixed, like unsalvagable boxes in corners
	int i;
	for (i = 0; i < WIDTH*HEIGHT; i++) if (level[i] & BOX) if (!(level[i] & GOAL)) {
		bool horizontally_fixed = (level[i + LEFT] & WALL) || (level[i + RIGHT] & WALL);
		bool vertically_fixed = (level[i + UP] & WALL) || (level[i + DOWN] & WALL);
		if (horizontally_fixed && vertically_fixed) return true;
	}
	return false;
}

bool level_is_solved(char* level) {
	int i;
	for (i = 0; i < WIDTH*HEIGHT; i++) if (level[i] & BOX) if (!(level[i] & GOAL)) return false;
	return true;
}

char sok_to_native(char sok) {
	if (sok == ' ') return EMPTY;
	if (sok == '#') return WALL;
	if (sok == '.') return GOAL;
	if (sok == '@') return PLAYER;
	if (sok == '$') return BOX;
	if (sok == '+') return GOAL | PLAYER;
	if (sok == '*') return GOAL | BOX;
	printf("Couldn't translate SOK character '%c' (%d)\n", sok, sok);
	exit(EXIT_FAILURE);
}

char native_to_sok(char native) {
	native &= ~ORIGINAL_PLAYER; //ignore ORIGINAL_PLAYER tag
	if (native == EMPTY) return ' ';
	if (native == WALL) return '#';
	if (native == GOAL) return '.';
	if (native == PLAYER) return '@';
	if (native == BOX) return '$';
	if (native == (GOAL | PLAYER)) return '+';
	if (native == (GOAL | BOX)) return '*';
	printf("Couldn't translate native tile byte %d\n", native);
	exit(EXIT_FAILURE);
}

void print_level(char* level) {
	int y, x;
	for (y = 0; y < HEIGHT; y++) {
		for (x = 0; x < WIDTH; x++)
			printf("%c", native_to_sok(level[y*WIDTH+x]));
		printf("\n");
	}
	printf("\n");
}

void print_state(struct game_state_node* node) {
	printf("[State %d, g = %d, h = %d]\n", node->H, node->g_score, node->h_score);
	print_level(node->level);
	/* if (level_is_solved(node->level)) {
		printf("This is a solution!");
		exit(EXIT_SUCCESS);
	} */
}

int find_and_remove_player(char* level) {
	int player_position;
	for (player_position = 0; !(level[player_position] & PLAYER); player_position++);
	level[player_position] &= ~PLAYER;
	level[player_position] |= ORIGINAL_PLAYER;
	return player_position;
}
void player_spread_recursive(char* level, int player_position) {
	if (level[player_position] & WALL) return;
	if (level[player_position] & PLAYER) return;
	if (level[player_position] & BOX) return;
	level[player_position] |= PLAYER;
	player_spread_recursive(level, player_position + UP);
	player_spread_recursive(level, player_position + DOWN);
	player_spread_recursive(level, player_position + LEFT);
	player_spread_recursive(level, player_position + RIGHT);
}
void player_spread(char* level) {
	player_spread_recursive(level, find_and_remove_player(level));
}

void remove_players(char* level) { //also removes original_players
	int i;
	for (i = 0; i < WIDTH*HEIGHT; i++) level[i] &= ~(PLAYER | ORIGINAL_PLAYER);
}

void populate_futures(struct game_state_node* node) { //find every possible future, make game_state_nodes for them, and make this node point to them
	char* level = node->level;
	node->futures = (struct game_state_node**) malloc(sizeof(struct game_state_node*) * MAX_POSSIBLE_FUTURES);
	int f;
	for (f = 0; f < MAX_POSSIBLE_FUTURES; f++) node->futures[f] = NULL;
	f = 0;
	// find every box. for each direction, if player could push it like that, make a copy where that happens
	int box_position;
	int d;
	for (box_position = 0; box_position < WIDTH*HEIGHT; box_position++) if (level[box_position] & BOX) {
		for (d = 0; d < 4; d++) {
			int dir = DIRECTION[d];
			if (level[box_position - dir] & PLAYER) if (!(level[box_position + dir] & WALL) && !(level[box_position + dir] & BOX)) {
				char* new_level = copy_level(level);
				remove_players(new_level);
				new_level[box_position + dir] |= BOX;
				new_level[box_position] &= ~BOX;
				new_level[box_position] |= PLAYER;
				new_level[box_position] |= ORIGINAL_PLAYER;
				new_level[box_position - dir] &= ~PLAYER;
				player_spread(new_level);
				if (level_is_already_dud(new_level)) {
					free(new_level);
				} else {
					node->futures[f++] = make_game_state_node(new_level);
				}
			}
		}
	}
}

void print_pathfind_without_touching_boxes_recursive_report(int* previous, char* how_we_got_there, int node) {
	//printf(":%d\n", node);
	if (node >= 0) {
		print_pathfind_without_touching_boxes_recursive_report(previous, how_we_got_there, previous[node]);
		if (how_we_got_there[node]) printf("%c", how_we_got_there[node]);
	}
}
void print_pathfind_without_touching_boxes(char* level, int start, int end) { //print lowercase. assume start and end are not-box and not-wall
	if ((level[start] & BOX) || (level[end] & BOX)) {
		printf("Pathfinding request within state starts or ends on box\n");
		exit(EXIT_FAILURE);
	}
	if (start == end) return;
	int i;
	int* queue = (int*) malloc(sizeof(int) * WIDTH * HEIGHT);
	for (i = 0; i < WIDTH*HEIGHT; i++) queue[i] = -1;
	int* previous = (int*) malloc(sizeof(int) * WIDTH * HEIGHT);
	for (i = 0; i < WIDTH*HEIGHT; i++) previous[i] = -1;
	char* how_we_got_there = (char*) malloc(sizeof(char) * WIDTH * HEIGHT);
	for (i = 0; i < WIDTH*HEIGHT; i++) how_we_got_there[i] = '\0';
	
	int q = 0; //end index for queue
	int s = 0; //start index for queue
	
	//set initial
	queue[q++] = start;
	previous[start] = -2; //SUPER special. don't make this point back to anywhere else, because it is the start!
	
	//printf("pathfinding from %d to %d\n", start, end);
	while (true) {
		int pick = queue[s++];
		if (pick < 0) {
			printf("Pathfinding within a step failed\n");
			//print_level(level);
			exit(EXIT_FAILURE);
		}
		//for every neighbor of pick, if they don't yet have a previous, then they should go through us! add them to the queue!
		int d;
		//printf("pick %d\n", pick);
		for (d = 0; d < 4; d++) {
			int neighbor = pick + DIRECTION[d];
			if (!(level[neighbor] & WALL)) if (!(level[neighbor] & BOX)) {
				if (previous[neighbor] == -1) {
					previous[neighbor] = pick;
					how_we_got_there[neighbor] = LURD[d] + ('a' - 'A');
					queue[q++] = neighbor;
					if (neighbor == end) {
						//printf("BFS FOUND LE PATH\n");
						print_pathfind_without_touching_boxes_recursive_report(previous, how_we_got_there, end);
						free(queue);
						free(how_we_got_there);
						free(previous);
						return;
					}
				}
			}
		}
	}
	
	free(queue);
	free(how_we_got_there);
	free(previous);
	
	printf("Couldn't perform pathfinding within a state\n");
	exit(EXIT_FAILURE);
}

void print_solution(struct game_state_node* state) {
	if (state->previous) {
		print_solution(state->previous);
		//printf("Now the transition from %d to %d\n", state->previous->H, state->H);
		//These are kinda computationally expensive but it's only O(solution length) so it's totally fine
		int original_player_tile = find_original_player(state->previous->level);
		int helper_1 = find_location_of_box_that_moved(state->previous->level, state->level);
		int helper_2 = find_location_of_box_that_moved(state->level, state->previous->level);
		int transition_offset = helper_2 - helper_1;
		print_pathfind_without_touching_boxes(state->previous->level, original_player_tile, helper_1-transition_offset);
		if (transition_offset == LEFT) printf("L");
		if (transition_offset == UP) printf("U");
		if (transition_offset == RIGHT) printf("R");
		if (transition_offset == DOWN) printf("D");
	}
	//print_state(state);
}

int main(int argc, char** argv) {
	FILENAME = argv[1];
	int i, j;
	
	// Find maximum row length, determines WIDTH. also find HEIGHT
	WIDTH = 0;
	HEIGHT = 0;
	FILE* fptr = get_fptr();
	char line[MAX_SOK_ROW_LENGTH];
	while (fgets(line, MAX_SOK_ROW_LENGTH, fptr)) {
		int string_length = 0; //string length, excluding whitespace
		while (line[string_length] != '\0' && line[string_length] != '\n') string_length++;
		if (string_length > WIDTH) WIDTH = string_length;
		HEIGHT++;
	}
	fclose(fptr);
	
	// Establish directions
	LEFT = DIRECTION[0] = -1;
	UP = DIRECTION[1] = -WIDTH;
	RIGHT = DIRECTION[2] = 1;
	DOWN = DIRECTION[3] = WIDTH;
	
	// Read into level array
	char* level = (char*) malloc (sizeof(char) * WIDTH * HEIGHT);
	for (i = 0; i < WIDTH * HEIGHT; i++) level[i] = EMPTY;
	N_BOXES = 0;
	N_GOALS = 0;
	int y, x;
	fptr = get_fptr();
	for (y = 0; y < HEIGHT; y++) {
		fgets(line, MAX_SOK_ROW_LENGTH, fptr);
		for (x = 0; x < WIDTH; x++) {
			if (line[x] == '\0') break;
			if (line[x] == '\n') break;
			level[y*WIDTH+x] = sok_to_native(line[x]);
			if (level[y*WIDTH+x] & BOX) N_BOXES++;
			if (level[y*WIDTH+x] & GOAL) N_GOALS++;
		}
	}
	fclose(fptr);
	
	//TODO: check that there is only one player, that the player is bounded by walls
	if (N_GOALS != N_BOXES) {
		printf("There are %d goals but %d boxes\n", N_GOALS, N_BOXES);
		exit(EXIT_FAILURE);
	}
	MAX_POSSIBLE_FUTURES = N_BOXES * 4 + 1; //maximum number of moves that can be made in any state; always strictly less, so that last pointer can be null
	GOAL_POSITIONS = (int*) malloc(sizeof(int) * N_GOALS);
	int g = 0;
	for (i = 0; g < N_GOALS; i++)
		if (level[i] & GOAL) GOAL_POSITIONS[g++] = i;
	BOX_POSITIONS = (int*) malloc(sizeof(int) * N_BOXES);
	
	// Compute walking-distance matrix. Used for heuristics. Ignore boxes and goals, only care about walls vs not walls
	// Floyd and Warshall say hi
	WALKING_DISTANCE_MATRIX = (int**) malloc(sizeof(int*) * WIDTH * HEIGHT);
	for (i = 0; i < WIDTH*HEIGHT; i++) {
		WALKING_DISTANCE_MATRIX[i] = (int*) malloc(sizeof(int) * WIDTH * HEIGHT);
		for (j = 0; j < WIDTH*HEIGHT; j++) WALKING_DISTANCE_MATRIX[i][j] = INT_MAX;
		WALKING_DISTANCE_MATRIX[i][i] = 0;
	}
	for (i = 0; i < WIDTH*HEIGHT; i++) if (!(level[i] & WALL)) for (j = 0; j < WIDTH*HEIGHT; j++) if (!(level[j] & WALL)) WALKING_DISTANCE_MATRIX[i][j] = 1;
	int k;
	for (k = 0; k < WIDTH*HEIGHT; k++) if (!(level[k] & WALL)) {
		for (i = 0; i < WIDTH*HEIGHT; i++) if (!(level[i] & WALL)) {
			for (j = 0; j < WIDTH*HEIGHT; j++) if (!(level[j] & WALL)) {
				int contender_distance = WALKING_DISTANCE_MATRIX[i][k] + WALKING_DISTANCE_MATRIX[k][j];
				if (contender_distance < WALKING_DISTANCE_MATRIX[i][j])
					WALKING_DISTANCE_MATRIX[i][j] = contender_distance;
			}	
		}
	}
	print_level(level);
	
	player_spread(level);
	
	for (i = 0; i < STATES_HASH_TABLE_SIZE; i++) GAME_STATES_HASH_TABLE[i] = NULL;
	struct game_state_node* start_state = make_game_state_node(level);
	
	struct game_state_node** frontier = (struct game_state_node**) malloc(sizeof(struct game_state_node*) * MAX_FRONTIER_SIZE); //TODO : replace with a heap i guess, that somehow picks minimum G+H
	for (i = 0; i < MAX_FRONTIER_SIZE; i++) frontier[i] = NULL;
	
	frontier[0] = start_state;
	start_state->g_score = 0;
	start_state->g_score = 0;
	
	//printf("Beginning search...\n");
	clock_t begin = clock();
	
	int iterations = 0;
	
	while (true) {
		int smallest_f_score = INT_MAX;
		struct game_state_node* pick = NULL; //pick from frontier
		int pick_position_in_frontier;
		for (i = 0; i < MAX_FRONTIER_SIZE; i++) {
			if (frontier[i] == NULL) continue;
			int f_score = frontier[i]->g_score + frontier[i]->h_score;
			if (f_score < smallest_f_score) {
				smallest_f_score = f_score;
				pick = frontier[i];
				pick_position_in_frontier = i;
			}
		}
		if (pick == NULL) {
			printf("Frontier is empty\n");
			exit(EXIT_FAILURE);
		}
		if (iterations % 5000 == 0) {
			clock_t end = clock();
			double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
			if (time_spent > TIMEOUT_TIME) {
				printf("Took more than %d seconds, abandoning search\n", TIMEOUT_TIME);
				exit(EXIT_FAILURE);
			}
		}
		iterations++;
		//printf("Pick from frontier\n");
		//print_state(pick);
		if (level_is_solved(pick->level)) {
			clock_t end = clock();
			double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
			print_solution(pick);
			printf("\n(Found in %d ms)\n", (int)(time_spent*1000));
			exit(EXIT_SUCCESS);
		}
		frontier[pick_position_in_frontier] = NULL;
		populate_futures(pick);
		int f;
		for (f = 0; pick->futures[f]; f++) {
			struct game_state_node* future = pick->futures[f];
			int possible_g_score = pick->g_score + 1;
			if (possible_g_score < future->g_score) {
				future->previous = pick;
				future->g_score = possible_g_score;
				for (i = 0; i < MAX_FRONTIER_SIZE; i++) if (frontier[i] == future) break;
				if (i == MAX_FRONTIER_SIZE) { //future isn't in frontier, so put it there
					for (i = 0; i < MAX_FRONTIER_SIZE; i++) if (frontier[i] == NULL) {
						frontier[i] = future;
						break;
					}
					if (i == MAX_FRONTIER_SIZE) {
						printf("Ran out of space in frontier (so silly....)\n");
						exit(EXIT_FAILURE);
					}
				}
			}
		}
	}
}