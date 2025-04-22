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

int INITIAL_PLAYER_POSITION;
int* INITIAL_BOX_POSITIONS;
int* GOAL_POSITIONS;

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

#define FROM_LEFT_SIDE 'L'
#define FROM_RIGHT_SIDE 'R' //constants to describe which end a game state node came from

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
	struct gamestate* point_back;
	char origin_side;
	
	int h_score; //precomputed, heuristic
	int g_score;
	int f_score;
	bool ever_been_in_frontier;
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

int hungarian(int* weights, int n) {
	int total = 0;
	int i, j;
	for (i = 0; i < n; i++) {
		int min_seen = INT_MAX;
		for (j = 0; j < n; j++) if (weights[i*n+j] < min_seen) min_seen = weights[i*n+j];
		total = add(total, min_seen);
	}
	for (j = 0; j < n; j++) {
		int min_seen = INT_MAX;
		for (i = 0; i < n; i++) if (weights[i*n+j] < min_seen) min_seen = weights[i*n+j];
		total = add(total, min_seen);
	}
	return total / 2;
}



int level_heuristic(char* level, int origin_side) {
	int i;
	int j = 0;
	int* box_positions = (int*) malloc(sizeof(int) * N_BOXES);
	for (i = 0; i < SIZE; i++) if (level[i] & BOX) box_positions[j++] = i;
	
	int* weights = (int*) malloc(sizeof(int) * N_BOXES * N_BOXES);
	
	if (origin_side == FROM_LEFT_SIDE) {
		for (i = 0; i < N_BOXES; i++) for (j = 0; j < N_BOXES; j++)
			weights[i*N_BOXES+j] = BOX_PUSHING_DISTANCE_MATRIX[box_positions[i]][GOAL_POSITIONS[j]];
	} else if (origin_side == FROM_RIGHT_SIDE) {
		for (i = 0; i < N_BOXES; i++) for (j = 0; j < N_BOXES; j++)
			weights[i*N_BOXES+j] = BOX_PUSHING_DISTANCE_MATRIX[INITIAL_BOX_POSITIONS[i]][box_positions[j]];
	}
	int result = hungarian(weights, N_BOXES);
	free(box_positions);
	return result;
}

struct gamestate* make_gamestate(char* level, int origin_side) {
	int hash = level_hash(level);
	struct gamestate* walker = GAMESTATE_HASH_TABLE[hash % GAMESTATE_HASH_TABLE_SIZE];
	while (walker) {
		if (walker->hash == hash) if (identical_levels(walker->level, level)) {
			if (origin_side != walker->origin_side) {
				// printf("( From other side ! )\n");
			}
			free(level);
			return walker;
		}
		walker = walker->next_in_hash_table;
	}
	struct gamestate* new_node = (struct gamestate*) malloc(sizeof(struct gamestate));
	new_node->level = level;
	new_node->hash = hash;
	new_node->next_in_hash_table = GAMESTATE_HASH_TABLE[hash % GAMESTATE_HASH_TABLE_SIZE];
	new_node->origin_side = origin_side;
	new_node->point_back = NULL;
	new_node->h_score = level_heuristic(level, origin_side);
	new_node->g_score = INT_MAX;
	new_node->f_score = INT_MAX;
	new_node->ever_been_in_frontier = false;
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
	printf("[State %08X, %c]\n", state, state->origin_side);
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


void find_post_states(struct gamestate** list, struct gamestate* state) {
	int i, d, j;
	char* level = state->level;
	for (i = 0; i < GROWTH_FACTOR; i++) list[i] = NULL;
	int entries = 0;
	for (i = 0; i < SIZE; i++) if (level[i] & BOX) for (d = 0; d < 4; d++) {
		int before_box = i - DIRECTIONS[d];
		int after_box = i + DIRECTIONS[d];
		//before_box is player, after_box is empty space. push the box there
		if (!(level[before_box] & WALL)) if (!(level[before_box] & BOX)) if (level[before_box] & PLAYER)
			if (!(level[after_box] & WALL)) if (!(level[after_box] & BOX)) {
				char* new_level = copy_level(level);
				new_level[i] &= ~BOX;
				new_level[after_box] |= BOX;
				set_player_region(new_level, i);
				struct gamestate* new_state = make_gamestate(new_level, state->origin_side);
				bool already_found = false;
				for (j = 0; j < entries && !already_found; j++) if (list[j] == new_state) already_found = true;
				if (!already_found) list[entries++] = new_state;
			}
	}
}
void find_pre_states(struct gamestate** list, struct gamestate* state) {
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
				struct gamestate* new_state = make_gamestate(new_level, state->origin_side);
				bool already_found = false;
				for (j = 0; j < entries && !already_found; j++) if (list[j] == new_state) already_found = true;
				if (!already_found) list[entries++] = new_state;
			}
	}
}

struct gamestate** HEAP;
int HEAP_CAPACITY;
int HEAP_MEMBERS;
void setup_heap(int capacity) {
	HEAP_CAPACITY = capacity;
	HEAP = (struct gamestate**) malloc(sizeof(struct gamestate*) * HEAP_CAPACITY);
	int i;
	for (i = 0; i < HEAP_CAPACITY; i++)
		HEAP[i] = NULL;
	HEAP_MEMBERS = 0;
}
void expand_heap_capacity() {
	struct gamestate** new_heap = (struct gamestate**) malloc(sizeof(struct gamestate*) * HEAP_CAPACITY * 2);
	int i;
	for (i = 0; i < HEAP_CAPACITY; i++) new_heap[i] = HEAP[i];
	for (i = HEAP_CAPACITY; i < HEAP_CAPACITY*2; i++) new_heap[i] = NULL;
	HEAP_CAPACITY *= 2;
	free(HEAP);
	HEAP = new_heap;
}
void sift_down(int index) {
	int left = 2 * index + 1;
	int right = 2 * index + 2;
	int smallest = index;

	if (left < HEAP_MEMBERS && HEAP[left]->f_score < HEAP[smallest]->f_score) {
		smallest = left;
	}
	if (right < HEAP_MEMBERS && HEAP[right]->f_score < HEAP[smallest]->f_score) {
		smallest = right;
	}
	if (smallest != index) {
		struct gamestate* temp = HEAP[index];
		HEAP[index] = HEAP[smallest];
		HEAP[smallest] = temp;
		sift_down(smallest);
	}
}
void sift_up(int index) {
	int parent = (index - 1) / 2;
	if (index > 0 && HEAP[index]->f_score < HEAP[parent]->f_score) {
		struct gamestate* temp = HEAP[index];
		HEAP[index] = HEAP[parent];
		HEAP[parent] = temp;
		sift_up(parent);
	}
}
void add_to_heap(struct gamestate* state) {
	if (state->f_score > (INT_MAX/2)-50) return;
	state->ever_been_in_frontier = true;
	if (HEAP_MEMBERS >= HEAP_CAPACITY) {
		expand_heap_capacity();
	}
	HEAP[HEAP_MEMBERS] = state;
	HEAP_MEMBERS++;
	sift_up(HEAP_MEMBERS - 1);
	//printf("added to heap, f score %d, from %c\n", state->f_score, (state->origin_side == FROM_LEFT_SIDE) ? 'L' : 'R');
}
struct gamestate* heap_pop() {
	if (HEAP_MEMBERS == 0) return NULL;
	struct gamestate* top = HEAP[0];
	HEAP[0] = HEAP[HEAP_MEMBERS - 1];
	HEAP_MEMBERS--;
	sift_down(0);
	return top;
}
void heap_update_new_f_score(struct gamestate* state) {
	int i;
	for (i = 0; i < HEAP_MEMBERS; i++) if (state == HEAP[i]) {
		sift_up(i);
		sift_down(i);
		return;
	}
}
void print_heap() {
	printf("======\n");
	int i;
	for (i = 0; i < HEAP_MEMBERS; i++) printf("%08X (f = %d)\n", HEAP[i], HEAP[i]->f_score);
	printf("======\n");
}



int* PATHFIND_QUEUE;
int* PATHFIND_POINT_BACK;
void setup_pathfinding_structures() {
	PATHFIND_QUEUE = (int*) malloc(sizeof(int) * SIZE);
	PATHFIND_POINT_BACK = (int*) malloc(sizeof(int) * SIZE);
}
void pathfind_on_map(char* level, int start, int end) {
	if (start == end) return;
	int i;
	int q = 0; //point to end of PATHFIND_QUEUE
	int s = 0; //next item to be popped from PATHFIND_QUEUE
	for (i = 0; i < SIZE; i++) PATHFIND_POINT_BACK[i] = -1;
	
	int d;
	
	PATHFIND_QUEUE[q++] = end;
	PATHFIND_POINT_BACK[end] = -2;
	
	while (q > s) {
		int pick = PATHFIND_QUEUE[s++];
		for (d = 0; d < 4; d++) {
			int neighbor = pick + DIRECTIONS[d];
			if (PATHFIND_POINT_BACK[neighbor] != -1) continue;
			if (level[neighbor] & BOX) continue;
			if (level[neighbor] & WALL) continue;
			PATHFIND_POINT_BACK[neighbor] = pick;
			PATHFIND_QUEUE[q++] = neighbor;
			if (neighbor == start) {
				//we can follow PATHFIND_POINT_BACK all the way from the start to the end (end will point to -2)
				int current = start;
				int next = PATHFIND_POINT_BACK[current];
				while (true) {
					if (next != -2) {
						int dif = next - current;
						for (d = 0; d < 4; d++) if (DIRECTIONS[d] == dif) printf("%c", LURD[d] + 'a' - 'A');
						current = next;
						next = PATHFIND_POINT_BACK[next];
					} else break;
				}
				return;
			}
		}
	}
	printf("Pathfinding failed\n");
	exit(EXIT_FAILURE);
}
int find_og_player(char* level) {
	int i;
	for (i = 0; i < SIZE; i++) if (level[i] & OG_PLAYER) return i;
	printf("Level has no OG player\n");
	exit(EXIT_FAILURE);
}
int find_missing_box(char* level1, char* level2) { //find box that is in level1 but isn't in level2
	int i;
	for (i = 0; i < SIZE; i++) if (level1[i] & BOX) if (!(level2[i] & BOX)) return i;
	printf("No mismatched boxes\n");
	exit(EXIT_FAILURE);
}
void reconstruct_solution_make_transition(struct gamestate* state1, struct gamestate* state2) {
	//printf("(How to go from %08X to %08X?)\n", state1, state2);
	int og_player_start = find_og_player(state1->level);
	int og_player_end = find_og_player(state2->level);
	int box_before = find_missing_box(state1->level, state2->level);
	int box_after = find_missing_box(state2->level, state1->level);
	int push_direction = box_after - box_before;
	
	pathfind_on_map(state1->level, og_player_start, box_before-push_direction);
	int d;
	for (d = 0; d < 4; d++) if (DIRECTIONS[d] == push_direction) printf("%c", LURD[d]);
	pathfind_on_map(state2->level, box_before, og_player_end);
}
void reconstruct_solution_left(struct gamestate* state) {
	if (!state->point_back) return;
	reconstruct_solution_left(state->point_back);
	reconstruct_solution_make_transition(state->point_back, state);
}
void reconstruct_solution_right(struct gamestate* state) {
	if (!state->point_back) return;
	reconstruct_solution_make_transition(state, state->point_back);
	reconstruct_solution_right(state->point_back);
}





int main() {
	int i, j, k;
	
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
	DIRECTIONS[0] = LEFT = -1;
	DIRECTIONS[1] = UP = -WIDTH;
	DIRECTIONS[2] = RIGHT = 1;
	DIRECTIONS[3] = DOWN = WIDTH;
	
	//Read INPUT_SOK into a level
	char* start_level = (char*) malloc(sizeof(char) * SIZE);
	for (i = 0; i < SIZE; i++) start_level[i] = 0;
	N_BOXES = N_GOALS = 0;
	j = 0;
	for (i = 0; i < SIZE;) {
		c = INPUT_SOK[j++];
		if (c == '\n') {
			while (i%WIDTH) i++;
			continue;
		}
		if (c == EOF || c == '\0') break;
		start_level[i] = sok_to_native(c);
		if (start_level[i] & BOX) N_BOXES++;
		if (start_level[i] & GOAL) N_GOALS++;
		i++;
	}
	if (N_BOXES != N_GOALS) {
		printf("Found %d boxes and %d goals\n", N_BOXES, N_GOALS);
		exit(EXIT_FAILURE);
	}
	free(INPUT_SOK);
	
	clock_t begin_time = clock();
	
	//Compute walking distance matrix
	BOX_PUSHING_DISTANCE_MATRIX = (int**) malloc(sizeof(int*) * SIZE);
	for (i = 0; i < SIZE; i++) {
		BOX_PUSHING_DISTANCE_MATRIX[i] = (int*) malloc(sizeof(int) * SIZE);
		for (j = 0; j < SIZE; j++) BOX_PUSHING_DISTANCE_MATRIX[i][j] = INT_MAX;
		BOX_PUSHING_DISTANCE_MATRIX[i][i] = 0;
	}
	for (i = 0; i < SIZE; i++) if (!(start_level[i] & WALL)) {
		for (k = 0; k < 4; k++) {
			int post = i + DIRECTIONS[k];
			int pre = i - DIRECTIONS[k];
			if (pre < 0 || post < 0 || pre >= SIZE || post >= SIZE) continue;
			if (!(start_level[post] & WALL) && !(start_level[pre] & WALL)) BOX_PUSHING_DISTANCE_MATRIX[i][post] = 1;
		}
	}
	for (k = 0; k < SIZE; k++) if (!(start_level[k] & WALL)) {
		for (i = 0; i < SIZE; i++) if (!(start_level[i] & WALL)) {
			for (j = 0; j < SIZE; j++) if (!(start_level[j] & WALL)) {
				int contender_distance = add(BOX_PUSHING_DISTANCE_MATRIX[i][k], BOX_PUSHING_DISTANCE_MATRIX[k][j]);
				if (contender_distance < BOX_PUSHING_DISTANCE_MATRIX[i][j])
					BOX_PUSHING_DISTANCE_MATRIX[i][j] = contender_distance;
			}	
		}
	}
	
	//Setup hash table and stuff
	GAMESTATE_HASH_TABLE = (struct gamestate**) malloc(sizeof(struct gamestate*) * GAMESTATE_HASH_TABLE_SIZE);
	for (i = 0; i < GAMESTATE_HASH_TABLE_SIZE; i++) GAMESTATE_HASH_TABLE[i] = NULL;
	INITIAL_BOX_POSITIONS = (int*) malloc(sizeof(int) * N_BOXES);
	GOAL_POSITIONS = (int*) malloc(sizeof(int) * N_GOALS);
	j = 0;
	k = 0;
	for (i = 0; i < SIZE; i++) {
		if (start_level[i] & BOX) INITIAL_BOX_POSITIONS[j++] = i;
		if (start_level[i] & GOAL) GOAL_POSITIONS[k++] = i;
		if (start_level[i] & PLAYER) INITIAL_PLAYER_POSITION = i;
	}
	
	set_player_region(start_level, INITIAL_PLAYER_POSITION);
	
	//Create start state
	struct gamestate* start_state = make_gamestate(start_level, FROM_LEFT_SIDE);
	start_state->g_score = 0;
	start_state->f_score = start_state->h_score;
	
	//Create end states
	char* end_level_template = copy_level(start_level);
	for (i = 0; i < SIZE; i++) if (end_level_template[i] & BOX) end_level_template[i] &= ~BOX;
	for (i = 0; i < SIZE; i++) if (end_level_template[i] & GOAL) end_level_template[i] |= BOX;
	struct gamestate** end_states = (struct gamestate**) malloc(sizeof(struct gamestate**) * GROWTH_FACTOR);
	int end_states_count = 0;
	for (j = 0; j < GROWTH_FACTOR; j++) end_states[j] = NULL;
	for (i = 0; i < SIZE; i++) if (end_level_template[i] & BOX) for (k = 0; k < 4; k++) {
		int player_position = i + DIRECTIONS[k];
		if (end_level_template[player_position] & BOX) continue;
		if (end_level_template[player_position] & WALL) continue;
		char* new_level = copy_level(end_level_template);
		set_player_region(new_level, player_position);
		struct gamestate* an_end_state = make_gamestate(new_level, FROM_RIGHT_SIDE);
		//make sure this isn't an end state we already considered
		bool already_considered = false;
		for (j = 0; j < end_states_count && !already_considered; j++)
			if (end_states[j] == an_end_state) {
				already_considered = true;
			}
		if (!already_considered) {
			end_states[end_states_count] = an_end_state;
			an_end_state->g_score = 0;
			an_end_state->f_score = an_end_state->h_score;
			end_states_count++;
			//print_state(an_end_state);
		}
	}
	free(end_level_template);
	
	//Prepare heap
	setup_heap(1024);
	add_to_heap(start_state);
	for (i = 0; i < end_states_count; i++) add_to_heap(end_states[i]);
	if (end_states_count == 0) {
		printf("Couldn't create ending states\n");
		exit(EXIT_FAILURE);
	}
	if (HEAP_MEMBERS <= 1) {
		printf("Heap is empty for some reason\n");
		exit(EXIT_FAILURE);
	}
	
	struct gamestate** neighbors = (struct gamestate**) malloc(sizeof(struct gamestate*) * GROWTH_FACTOR);
	
	//A*, from both sides
	while (HEAP_MEMBERS) {
		//print_heap();
		struct gamestate* pick = heap_pop();
		//printf("Heap has %d members, chose something where f_score = %d\n", HEAP_MEMBERS, pick->f_score);
		//printf("\n\nPick\n");
		//print_state(pick);
		//printf("%c", pick->origin_side);
		
		if (pick->origin_side == FROM_LEFT_SIDE) find_post_states(neighbors, pick);
		if (pick->origin_side == FROM_RIGHT_SIDE) find_pre_states(neighbors, pick);
		
		for (i = 0; i < GROWTH_FACTOR && neighbors[i]; i++) {
			struct gamestate* neighbor = neighbors[i];
			if (neighbor->origin_side != pick->origin_side) {
				setup_pathfinding_structures();
				struct gamestate* towards_left = (pick->origin_side == FROM_LEFT_SIDE) ? pick : neighbor;
				struct gamestate* towards_right = (pick->origin_side == FROM_LEFT_SIDE) ? neighbor : pick;
				print_level(start_state->level);
				reconstruct_solution_left(towards_left);
				reconstruct_solution_make_transition(towards_left, towards_right);
				reconstruct_solution_right(towards_right);
				printf("\n");
				clock_t end_time = clock();
				printf("(%d ms)\n", (int)((double)(end_time - begin_time) / CLOCKS_PER_SEC * 1000));
				exit(EXIT_SUCCESS);
			}
			int possible_g_score = pick->g_score + 1;
			if (possible_g_score < neighbor->g_score) {
				neighbor->point_back = pick;
				neighbor->g_score = possible_g_score;
				neighbor->f_score = possible_g_score + neighbor->h_score;
				if (neighbor->ever_been_in_frontier) {
					heap_update_new_f_score(neighbor);
				} else {
					add_to_heap(neighbor);
				}
			}
		}
	}
	printf("Search failed\n");
	exit(EXIT_FAILURE);
}