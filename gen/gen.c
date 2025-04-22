#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define MAX_LEVEL_SIZE 65536

char* INPUT_SOK;

int WIDTH;
int HEIGHT;
#define SIZE (WIDTH*HEIGHT)
int N_BOXES;
int N_GOALS;

char* LURD = "LURD";

#define GROWTH_FACTOR (4*N_BOXES)

#define QUEUE_SIZE (65536*256)

int LEFT;
int UP;
int RIGHT;
int DOWN;
int DIRECTIONS[4];

int add(int a, int b) {
	if (a == INT_MAX) return INT_MAX;
	if (b == INT_MAX) return INT_MAX;
	return a + b;
}

#define MAX_FUTURES (4*N_BOXES)

int** BOX_PUSHING_DISTANCE_MATRIX; //M[i][j] is minimum # pushes to push a box from i to j

#define EMPTY (0)
#define GOAL (1)
#define PLAYER (2)
#define BOX (4)
#define WALL (8)
#define OG_PLAYER (16)

#define GAMESTATE_HASH_TABLE_SIZE (65536)
struct gamestate** GAMESTATE_HASH_TABLE;

struct gamestate {
	char* level;
	int hash;
	struct gamestate* next_in_hash_table;
	int complexity;
};

bool identical_levels(char* a, char* b) {
	for (int i = 0; i < SIZE; i++) if ((a[i] & ~OG_PLAYER) != (b[i] & ~OG_PLAYER)) return false;
	return true;
}

char* copy_level(char* level) {
	char* copy = (char*) malloc(sizeof(char) * SIZE);
	int i;
	for (i = 0; i < SIZE; i++) copy[i] = level[i];
	return copy;
}

int level_hash(char* level) {
	int i;
	int h = 0;
	for (i = 0; i < SIZE; i++) if (level[i] & BOX) h += i*i*i;
	for (i = 0; i < SIZE; i++) if (level[i] & PLAYER) return h + i;
	printf("Couldn't compute level hash - has no players?\n");
	exit(EXIT_FAILURE);
}

//RETURNS NULL IF GAMESTATE ALREADY EXISTS
struct gamestate* make_new_gamestate(char* level, int proposed_complexity) {
	int hash = level_hash(level);
	struct gamestate* walker = GAMESTATE_HASH_TABLE[hash % GAMESTATE_HASH_TABLE_SIZE];
	while (walker) {
		if (walker->hash == hash) if (identical_levels(walker->level, level)) {
			return NULL;
		}
		walker = walker->next_in_hash_table;
	}
	struct gamestate* new_node = (struct gamestate*) malloc(sizeof(struct gamestate));
	new_node->level = level;
	new_node->hash = hash;
	new_node->next_in_hash_table = GAMESTATE_HASH_TABLE[hash % GAMESTATE_HASH_TABLE_SIZE];
	new_node->complexity = proposed_complexity;
	GAMESTATE_HASH_TABLE[hash % GAMESTATE_HASH_TABLE_SIZE] = new_node;
	return new_node;
}

char sok_to_native(char sok) {
	if (sok == ' ') return EMPTY;
	if (sok == '#') return WALL;
	if (sok == '.') return GOAL;
	if (sok == '@') return PLAYER;
	if (sok == '$') return BOX;
	if (sok == '+') return GOAL | PLAYER;
	if (sok == '*') return GOAL | BOX;
	if (sok == '-') return EMPTY;
	if (sok == '_') return EMPTY;
	printf("Couldn't translate SOK character '%c' (%d)\n", sok, sok);
	exit(EXIT_FAILURE);
}

char native_to_sok(char native) {
	if (!(native & OG_PLAYER)) native &= ~PLAYER;
	native &= ~OG_PLAYER;
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

void print_state(struct gamestate* state) {
	printf("[State %08X]\n", state);
	print_level(state->level);
}

void recursive_player_spread(char* level, int location) {
	if (level[location] & PLAYER) return;
	level[location] |= PLAYER;
	int d;
	for (d = 0; d < 4; d++) {
		char adjacent_content = level[location + DIRECTIONS[d]];
		if (!(adjacent_content & WALL) && !(adjacent_content & BOX)) recursive_player_spread(level, location + DIRECTIONS[d]);
	}
}
void set_player_region(char* level, int player_position) {
	int i;
	for (i = 0; i < SIZE; i++) level[i] &= ~(PLAYER | OG_PLAYER);
	recursive_player_spread(level, player_position);
	level[player_position] |= OG_PLAYER;
}

void find_new_pre_states(struct gamestate** list, struct gamestate* state) {
	int i, d, j;
	char* level = state->level;
	for (i = 0; i < GROWTH_FACTOR; i++) list[i] = NULL;
	int entries = 0;
	for (i = 0; i < SIZE; i++) if (level[i] & BOX) for (d = 0; d < 4; d++) {
		int after_box = i + DIRECTIONS[d];
		int after_after_box = i + 2 * DIRECTIONS[d];
		//after_box is player, after_after_box is empty space. pull the box
		if (!(level[after_box] & WALL)) if (!(level[after_box] & BOX)) if (level[after_box] & PLAYER)
			if (!(level[after_after_box] & WALL)) if (!(level[after_after_box] & BOX)) {
				char* new_level = copy_level(level);
				new_level[i] &= ~BOX;
				new_level[after_box] |= BOX;
				set_player_region(new_level, after_after_box);
				struct gamestate* new_state = make_new_gamestate(new_level, state->complexity + 1);
				if (new_state) list[entries++] = new_state;
			}
	}
}

int main(int nargs, char** arglist) {
	int i, j;
	
	int SPAWN_GROUP_SIZE = atoi(arglist[1]);
	N_BOXES = N_GOALS = atoi(arglist[2]);
	
	//Set paramters
	srand(time(NULL));
	
	//Read sok into INPUT_SOK
	INPUT_SOK = (char*) malloc(sizeof(char) * MAX_LEVEL_SIZE);
	int current_row_width = 0;
	char c;
	i = 0;
	WIDTH = 0;
	HEIGHT = 0;
	do {
		c = getchar();
		INPUT_SOK[i++] = c;
		if (c == '\n') {
			if (current_row_width > WIDTH)
				WIDTH = current_row_width;
			current_row_width = 0;
			HEIGHT++;
		}
		else current_row_width++;
	} while (c != EOF && c != '\0');
	if (current_row_width > WIDTH) WIDTH = current_row_width;
	if (current_row_width) HEIGHT++;
	//WIDTH--;
	DIRECTIONS[0] = LEFT = -1;
	DIRECTIONS[1] = UP = -WIDTH;
	DIRECTIONS[2] = RIGHT = 1;
	DIRECTIONS[3] = DOWN = WIDTH;
	
	//Read INPUT_SOK into a level
	int initial_player_position = -1;
	char* level_template = (char*) malloc(sizeof(char) * SIZE);
	for (i = 0; i < SIZE; i++) level_template[i] = 0;
	j = 0;
	for (i = 0; i < SIZE;) {
		c = INPUT_SOK[j++];
		if (c == '\n') {
			while (i%WIDTH) i++;
			continue;
		}
		if (c == EOF || c == '\0') break;
		level_template[i] = sok_to_native(c);
		if (level_template[i] != WALL && level_template[i] != EMPTY) {
			if (level_template[i] == PLAYER) {
				//ok that's fine...
				initial_player_position = i;
				level_template[i] = EMPTY;
			} else {
				printf("Your inputted level shouldn't have anything other than walls\n");
				exit(EXIT_FAILURE);
			}
		}
		i++;
	}
	free(INPUT_SOK);
	
	if (initial_player_position == -1) {
		printf("You didn't provide a player position\n");
		exit(EXIT_FAILURE);
	}
	
	char* indicate_player_region = copy_level(level_template);
	set_player_region(indicate_player_region, initial_player_position);
	
	clock_t begin_time = clock();
	
	//Setup hash table and stuff
	GAMESTATE_HASH_TABLE = (struct gamestate**) malloc(sizeof(struct gamestate*) * GAMESTATE_HASH_TABLE_SIZE);
	for (i = 0; i < GAMESTATE_HASH_TABLE_SIZE; i++) GAMESTATE_HASH_TABLE[i] = NULL;
	
	struct gamestate** neighbors = (struct gamestate**) malloc(sizeof(struct gamestate*) * GROWTH_FACTOR);
	
	struct gamestate** queue = (struct gamestate**) malloc(sizeof(struct gamestate*) * QUEUE_SIZE);
	int push_spot = 0;
	int pop_spot = 0;
	int max_complexity_seen = -1;
	
	//Generate a bunch of levels based on level_template
	for (i = 0; i < SPAWN_GROUP_SIZE; i++) {
		char* level = copy_level(level_template);
		int things_placed = 0;
		int player_spot;
		for (j = 0; j < 100 && things_placed < N_BOXES+1; j++) { //j is # attempts at placing things
			int spot = rand() % SIZE;
			if (!(indicate_player_region[spot] & PLAYER)) continue;
			if (level[spot]) continue;
			if (things_placed) {
				level[spot] = BOX | GOAL;
			} else {
				level[spot] = PLAYER | OG_PLAYER;
				player_spot = spot;
			}
			things_placed++;
		}
		if (things_placed != N_BOXES+1) continue;
		
		set_player_region(level, player_spot);
		struct gamestate* state = make_new_gamestate(level, 0);
		if (state) {
			printf("Good one %d\n", push_spot);
			//print_level(level);
			queue[push_spot++] = state;
		} else {
			printf("Fail");
			free(level);
		}
	}
	
	printf("Made starting states\n");
	//exit(EXIT_FAILURE);
	
	struct gamestate* most_complex;
	while (push_spot < QUEUE_SIZE && pop_spot < push_spot) {
		struct gamestate* pick = queue[pop_spot++];
		find_new_pre_states(neighbors, pick);
		for (i = 0; i < GROWTH_FACTOR && neighbors[i]; i++) {
			//printf("%08X go to %08X perhaps\n", pick, neighbors[i]);
			queue[push_spot++] = neighbors[i];
			if (neighbors[i]->complexity > max_complexity_seen) {
				max_complexity_seen = neighbors[i]->complexity;
				printf("[%d]\n", max_complexity_seen);
				print_level(neighbors[i]->level);
				most_complex = neighbors[i];
			}
		}
	}
	printf("\n\n\n");
	if (push_spot == QUEUE_SIZE) printf("Queue full, so stopping search\n");
	else printf("Found absolute maximum shuffle!\n");
	exit(EXIT_FAILURE);
}