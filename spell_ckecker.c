#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define MAX_CANDID 10 // set the maximum number of spelling candidates
#define MAX_WORD_LEN 256 

const char *dict = "dict.txt"; // replace this with the path to your dictionnary

// spelling candidate 
typedef struct {
    char *word;
    int dis;
} candidate;

// entry of the associative array (aka hash map)
typedef struct entry {
    char *key;
    int value;
    struct entry *next;
} entry;

typedef struct map {
    entry **buckets; // buckets to store entries
    int capacity; // custom
} map;

// hashing function for the indexing
unsigned long long hash(const char *str) {
    unsigned long long h = 5381;
    int c;
    while ((c = *str++)) {
        h = ((h << 5) + h) + c;
    }
    return h;
}

map *create_map(int capacity) {
    map *m = (map *)malloc(sizeof(map));
    if (!m) return NULL;

    m->capacity = capacity;
    m->buckets = (entry**)calloc(capacity, sizeof(entry*)); // allocate memory for each bucket collectively
    if (!m->buckets) {
        free(m);
        return NULL;
    }

    return m;
}

// get a specific key's value
int get(map *m, char *key) {
    int index = hash(key) % m->capacity; // get the index of the key
    entry *e = m->buckets[index]; 

    // iterate through the map till finding key and return it's value
    while (e != NULL) {
        if (strcmp(key, e->key) == 0) {
            return e->value;
        }
        e = e->next;
    }
    // if no such key is found
    return 0;
}

// add a new entry to the map 
void put(map *m, const char *key, int value) {
    int index = hash(key) % m->capacity; // generate an index for the key
    entry *e = m->buckets[index]; // make an entry at the index

    // iterate to find a potential matching key
    while (e != NULL) {
        if (strcmp(e->key, key) == 0) {
            // if key is found replace the value with new one
            e->value = value; 
            return;
        }
        e = e->next;
    }

    // if no existing entry with the given key 
    // is found then we should make a new entry
    entry *newEntry = (entry *)malloc(sizeof(entry));
    if (!newEntry) return;

    // intialize the new entry
    newEntry->value = value;
    newEntry->key = strdup(key);
    if (!newEntry->key) {
        free(newEntry);
        return;
    }

    // insert the new entry at the calculated index
    newEntry->next = m->buckets[index];
    m->buckets[index] = newEntry;
}

// free the map so you don't shoot yourself in the foot
void free_map(map *m) {
    for (int i = 0; i < m->capacity; i++) {
        entry *e = m->buckets[i];
        while (e) {
            entry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(m->buckets);
    free(m);
}

int sub_min(int a, int b) {
    return a < b ? a : b;
}

// simply gets the maximum between three numbers
int min_num(int a, int b, int c) {
    int x = sub_min(a, b);
    int y = sub_min(a, c);
    return sub_min(x, y);
}

// get the edit distance between two input strings
/*
    the edit distance is the minimum number of operations (insert, delete, substitute)
    needed to transform the first string into the second string, it is particularly
    useful in this case to determine the closest candidates for spelling mistakes by 
    calculating the edit distance between candidates and applying additional checks
*/
int edit_distance(char *s, char *t) {
    int m = strlen(s);
    int n = strlen(t);

    // using a table of distances to keep track of the edit distance
    int **dp = (int **)malloc((m + 1) * sizeof(int *));
    if (!dp) return -1;

    // allocating memory for each row to be 'n' length
    for (int i = 0; i <= m; i++) {
        dp[i] = (int *)malloc((n + 1)*sizeof(int));
        if (!dp[i]) {
            // free the previously allocated 
            // rows before failing
            for (int j = 0; j < i; j++) { 
                free(dp[j]); 
            }
            free(dp);
            return -1;
        }
    }

    // initialize rows
    for (int i = 0; i <= m; i++) {
        dp[i][0] = i;
    }

    for (int j = 0; j <= n; j++) {
        dp[0][j] = j;
    }

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            int sub_cost = (s[i - 1] == t[j - 1]) ? 0 : 1; // should we substitute this char
            // mark the minimum number of operations required 
            dp[i][j] = min_num(dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + sub_cost);
        }
    }

    int result = dp[m][n];

    // free the memory 
    for (int i = 0; i <= m; i++) {
        free(dp[i]);
    }
    free(dp);

    return result;
}

// find a string in the dictionnary using binary search
int binary_search_file(FILE *file, const char *target) {
    long low = 0;
    fseek(file, 0, SEEK_END);
    long high = ftell(file);
    char buffer[MAX_WORD_LEN]; 
    int line_num = 0;

    while (low <= high) {
        long mid = low + (high - low) / 2;
        fseek(file, mid, SEEK_SET);

        if (mid != 0) {
            fgets(buffer, MAX_WORD_LEN, file);
        }

        long pos = ftell(file);
        if (fgets(buffer, MAX_WORD_LEN, file) == NULL) break;

        buffer[strcspn(buffer, "\n")] = '\0';
        int cmp = strcmp(buffer, target);

        if (cmp == 0) {
            fseek(file, 0, SEEK_SET);
            line_num = 0;
            while (ftell(file) < pos) {
                if (fgets(buffer, MAX_WORD_LEN, file) == NULL) break;
                line_num++;
            }
            return line_num + 1;
        } else if (cmp > 0) {
            high = mid - 1;
        } else {
            low = ftell(file);
        }
    }
    return -1;
}

// if a word isn't in the dictionnary then search for 
// the position where it should be inserted just to get 
// the words that are around that position as candidates
int binary_search_insert(FILE *file, const char *target) {
    long low = 0;
    fseek(file, 0, SEEK_END);
    long high = ftell(file);
    char buffer[MAX_WORD_LEN];
    int line_number = 0;

    while (low <= high) {
        long mid = low + (high - low) / 2;
        fseek(file, mid, SEEK_SET);

        if (mid != 0) {
            fgets(buffer, MAX_WORD_LEN, file);
        }

        long position = ftell(file);
        if (!fgets(buffer, MAX_WORD_LEN, file)) break;

        buffer[strcspn(buffer, "\n")] = '\0';

        int cmp = strcmp(buffer, target);

        if (cmp == 0) {
            fseek(file, 0, SEEK_SET);
            line_number = 0;
            while (ftell(file) < position) {
                if (fgets(buffer, MAX_WORD_LEN, file) == NULL) break;
                line_number++;
            }
            return line_number + 1;
        } else if (cmp > 0) {
            high = mid - 1;
        } else {
            low = ftell(file);
        }
    }

    fseek(file, 0, SEEK_SET);
    line_number = 0;
    while (fgets(buffer, MAX_WORD_LEN, file) != NULL) {
        if (strcmp(buffer, target) > 0) {
            return line_number + 1;
        }
        line_number++;
    }
    return line_number + 1;
}

char *get_word_by_line_number(FILE *file, int line_number) {
    static char buffer[MAX_WORD_LEN];
    int current_line = 0;

    fseek(file, 0, SEEK_SET);
    while (fgets(buffer, MAX_WORD_LEN, file) != NULL) {
        if (++current_line == line_number) {
            buffer[strcspn(buffer, "\n")] = '\0';
            return buffer;
        }
    }
    return NULL;
}

// sorting the candidates array
void insert_sorted(candidate *arr, int *size, candidate word) {
    int pos = 0;
    while (pos < *size && word.dis >= arr[pos].dis) pos++;

    for (int i = *size; i > pos; i--) arr[i] = arr[i - 1];

    arr[pos] = word;
    (*size)++;
}

// prioritize candidates that have input as the prefix
int is_prefix(char *word, char *candidate) {
    return strncmp(word, candidate, strlen(word)) == 0;
}

// check if the word is in the dictionnary
bool isCorrectSpell(char *word) {
    FILE *fp = fopen(dict, "r");
    if (!fp) {
        return false;
    }

    int index = binary_search_file(fp, word);
    fclose(fp);

    return index != -1;
}

// using what we implemented we look for the spelling candidates
char **get_spelling_candidates(char *word, int *returnSize) {
    char **candidates = (char **)malloc(MAX_CANDID * sizeof(char *));
    if (!candidates) {
        fprintf(stderr, "Memory allocation failed : get_spelling_candidates()\n");
        return NULL;
    }

    map *m = create_map(MAX_CANDID);
    if (!m) {
        free(candidates);
        fprintf(stderr, "Memory allocation failed : get_spelling_candidates()\n");
        return NULL;
    }

    FILE *fp = fopen(dict, "r");
    if (!fp) {
        free_map(m);
        free(candidates);
        fprintf(stderr, "File opening failed : get_spelling_candidates()\n");
        return NULL;
    }

    int index = binary_search_insert(fp, word);
    int j = 0;
    for (int i = index - 4; i <= index + 4 && j < MAX_CANDID; i++) {
        if (i < 1) continue;

        char *candid = get_word_by_line_number(fp, i);

        if (candid == NULL) continue;
        if (candid[0] != word[0]) continue;

        int dis = edit_distance(word, candid);

        if (!is_prefix(word, candid)) {
            dis += 10;
        }

        if (dis == -1) {
            dis = edit_distance(word, candid);
            if (dis == -1) {
                fclose(fp);
                for (int z = 0; z < j; z++) {
                    free(candidates[z]);
                }
                free_map(m);
                free(candid);
                return NULL;
            }
        }

        candidates[j] = strdup(candid);
        if (!candidates[j]) continue;

        put(m, candidates[j], dis);

        j++;
    }

    candidate *cans = (candidate *)malloc(MAX_CANDID * sizeof(candidate));
    if (!cans) {
        free_map(m);
        for (int i = 0; i < j; i++) {
            free(candidates[i]);
        }
        free(candidates);
        fclose(fp);
        fprintf(stderr, "Memory allocation failed : get_spelling_candidates()\n");
        return NULL;
    }

    int size = 0;
    for (int i = 0; i < j; i++) {
        candidate can;
        can.word = candidates[i];
        can.dis = get(m, can.word);
        insert_sorted(cans, &size, can);
    }

    for (int i = 0; i < size; i++) {
        candidates[i] = strdup(cans[i].word);
        (*returnSize)++;
    }

    for (int i = size; i < MAX_CANDID; i++) {
        candidates[i] = NULL;
    }

    free_map(m);
    free(cans);
    fclose(fp);

    return candidates;
}


// spelling checker
void spell_check(char *word) {
    if (!isCorrectSpell(word)) {
        int size = 0;
        char **candidates = get_spelling_candidates(word, &size);
        if (!candidates) {
            fprintf(stderr, "Failed to get spelling candidates!\n");
            return;
        }

        for (int i = 0; i < size; i++) {
            printf("%s\n", candidates[i]);
            free(candidates[i]);
        }
        free(candidates);
    } else {
        printf("word spelling is correct!\n");
    }
}

int main() {
    char *s = (char *)malloc(NAME_MAX * sizeof(char));
    if (!s) {
        fprintf(stderr, "Memory allocation error!\n");
        return -1;
    }

    printf("Enter word : ");
    if (!fgets(s, NAME_MAX, stdin)) {
        free(s);
        fprintf(stderr, "Failed to read input!\n");
        return -1;
    }

    s[strcspn(s, "\n")] = '\0';

    spell_check(s);

    free(s);

    return 0;
}
