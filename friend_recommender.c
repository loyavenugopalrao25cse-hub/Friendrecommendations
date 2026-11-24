/*
 * Friend recommendation on a social network using graphs (C version).
 *
 * Users are nodes, friendships are edges (undirected graph stored as an
 * adjacency matrix). Candidates are friends-of-friends (2 hops away) who are
 * not already friends. They are ranked by one of three scores:
 *   MUTUAL  : number of mutual friends
 *   JACCARD : mutual / total distinct friends of both
 *   ADAMIC  : Adamic-Adar, sum of 1/log(degree) over mutual friends
 *
 * Compile:  gcc friend_recommender.c -o friend_recommender -lm
 * Run:      ./friend_recommender
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_USERS 100
#define NAME_LEN  32

typedef enum { MUTUAL, JACCARD, ADAMIC } Method;

typedef struct {
    int  n;                                  /* number of users */
    char names[MAX_USERS][NAME_LEN];
    int  adj[MAX_USERS][MAX_USERS];          /* 1 if friends, else 0 */
} Graph;

typedef struct {
    int    id;
    double score;
} Candidate;

/* ---------- graph construction ---------- */

void graph_init(Graph *g) {
    memset(g, 0, sizeof(Graph));
}

/* Returns the id of a user, adding the user if new. */
int get_user(Graph *g, const char *name) {
    for (int i = 0; i < g->n; i++)
        if (strcmp(g->names[i], name) == 0)
            return i;
    if (g->n >= MAX_USERS) {
        fprintf(stderr, "Too many users (max %d)\n", MAX_USERS);
        exit(1);
    }
    strncpy(g->names[g->n], name, NAME_LEN - 1);
    return g->n++;
}

void add_friendship(Graph *g, const char *a, const char *b) {
    int u = get_user(g, a);
    int v = get_user(g, b);
    if (u == v) return;
    g->adj[u][v] = 1;
    g->adj[v][u] = 1;
}

int degree(const Graph *g, int u) {
    int d = 0;
    for (int i = 0; i < g->n; i++) d += g->adj[u][i];
    return d;
}

/* ---------- scoring functions ---------- */

int mutual_count(const Graph *g, int u, int v) {
    int c = 0;
    for (int z = 0; z < g->n; z++)
        if (g->adj[u][z] && g->adj[v][z]) c++;
    return c;
}

double jaccard(const Graph *g, int u, int v) {
    int inter = 0, uni = 0;
    for (int z = 0; z < g->n; z++) {
        if (g->adj[u][z] && g->adj[v][z]) inter++;
        if (g->adj[u][z] || g->adj[v][z]) uni++;
    }
    return uni ? (double)inter / uni : 0.0;
}

double adamic_adar(const Graph *g, int u, int v) {
    double score = 0.0;
    for (int z = 0; z < g->n; z++) {
        if (g->adj[u][z] && g->adj[v][z]) {
            int d = degree(g, z);
            if (d > 1) score += 1.0 / log((double)d);
        }
    }
    return score;
}

double compute_score(const Graph *g, int u, int v, Method m) {
    switch (m) {
        case MUTUAL:  return (double)mutual_count(g, u, v);
        case JACCARD: return jaccard(g, u, v);
        default:      return adamic_adar(g, u, v);
    }
}

/* ---------- candidate generation (depth-2 search) ---------- */

/* Fills out[] with friends-of-friends of u who are not u or direct friends.
 * Returns the number of candidates. */
int find_candidates(const Graph *g, int u, int out[]) {
    int seen[MAX_USERS] = {0};
    int count = 0;
    for (int f = 0; f < g->n; f++) {
        if (!g->adj[u][f]) continue;             /* f is a direct friend */
        for (int fof = 0; fof < g->n; fof++) {
            if (g->adj[f][fof] && fof != u && !g->adj[u][fof] && !seen[fof]) {
                seen[fof] = 1;
                out[count++] = fof;
            }
        }
    }
    return count;
}

/* ---------- ranking ---------- */

static const Graph *sort_graph;                   /* used by qsort comparator */

int cmp_candidates(const void *a, const void *b) {
    const Candidate *x = a, *y = b;
    if (x->score > y->score) return -1;
    if (x->score < y->score) return 1;
    return strcmp(sort_graph->names[x->id], sort_graph->names[y->id]);
}

void recommend(const Graph *g, const char *user, int k, Method m) {
    int u = -1;
    for (int i = 0; i < g->n; i++)
        if (strcmp(g->names[i], user) == 0) u = i;
    if (u < 0) {
        printf("User '%s' not found.\n", user);
        return;
    }

    int ids[MAX_USERS];
    int n = find_candidates(g, u, ids);

    Candidate cand[MAX_USERS];
    for (int i = 0; i < n; i++) {
        cand[i].id = ids[i];
        cand[i].score = compute_score(g, u, ids[i], m);
    }

    sort_graph = g;
    qsort(cand, n, sizeof(Candidate), cmp_candidates);

    if (k > n) k = n;
    for (int i = 0; i < k; i++) {
        int v = cand[i].id;
        printf("  %-6s score=%.3f  mutual friends=[", g->names[v], cand[i].score);
        int first = 1;
        for (int z = 0; z < g->n; z++) {
            if (g->adj[u][z] && g->adj[v][z]) {
                printf("%s%s", first ? "" : ", ", g->names[z]);
                first = 0;
            }
        }
        printf("]\n");
    }
}

/* ---------- demo ---------- */

int main(void) {
    Graph g;
    graph_init(&g);

    const char *edges[][2] = {
        {"Alice", "Bob"},   {"Alice", "Carol"}, {"Alice", "David"},
        {"Bob", "Carol"},   {"Bob", "Eve"},
        {"Carol", "Eve"},   {"Carol", "Frank"},
        {"David", "Frank"}, {"David", "Grace"},
        {"Eve", "Heidi"},
        {"Frank", "Heidi"}, {"Frank", "Ivan"},
        {"Grace", "Ivan"},
    };
    int num_edges = sizeof(edges) / sizeof(edges[0]);
    for (int i = 0; i < num_edges; i++)
        add_friendship(&g, edges[i][0], edges[i][1]);

    const char *user = "Alice";
    int uid = get_user(&g, user);

    printf("%s's friends: ", user);
    int first = 1;
    for (int i = 0; i < g.n; i++) {
        if (g.adj[uid][i]) {
            printf("%s%s", first ? "" : ", ", g.names[i]);
            first = 0;
        }
    }
    printf("\n\n");

    printf("Recommendations (mutual):\n");
    recommend(&g, user, 5, MUTUAL);
    printf("\nRecommendations (jaccard):\n");
    recommend(&g, user, 5, JACCARD);
    printf("\nRecommendations (adamic):\n");
    recommend(&g, user, 5, ADAMIC);

    return 0;
}
