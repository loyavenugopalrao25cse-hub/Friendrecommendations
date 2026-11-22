"""
Friend recommendation on a social network using graphs.

The network is an undirected graph: users are nodes, friendships are edges.
Candidates are friends-of-friends (2 hops away) who are not already friends.
They are ranked by one of three scores:
  - mutual : number of mutual friends
  - jaccard: mutual / total distinct friends of both (normalizes for popularity)
  - adamic : Adamic-Adar, mutual friends weighted by 1/log(their degree),
             so a shared friend with few connections counts more than a hub
"""

import math
from collections import defaultdict


class SocialGraph:
    def __init__(self):
        self.adj = defaultdict(set)

    def add_friendship(self, a, b):
        if a == b:
            return
        self.adj[a].add(b)
        self.adj[b].add(a)

    def friends(self, user):
        return self.adj[user]

    # ---------- candidate generation (BFS, depth 2) ----------
    def candidates(self, user):
        direct = self.adj[user]
        result = set()
        for friend in direct:
            for fof in self.adj[friend]:
                if fof != user and fof not in direct:
                    result.add(fof)
        return result

    # ---------- scoring functions ----------
    def mutual_count(self, u, v):
        return len(self.adj[u] & self.adj[v])

    def jaccard(self, u, v):
        union = self.adj[u] | self.adj[v]
        return len(self.adj[u] & self.adj[v]) / len(union) if union else 0.0

    def adamic_adar(self, u, v):
        score = 0.0
        for z in self.adj[u] & self.adj[v]:
            deg = len(self.adj[z])
            if deg > 1:
                score += 1 / math.log(deg)
        return score

    # ---------- recommendation ----------
    def recommend(self, user, k=5, method="adamic"):
        scorers = {
            "mutual": self.mutual_count,
            "jaccard": self.jaccard,
            "adamic": self.adamic_adar,
        }
        if method not in scorers:
            raise ValueError(f"method must be one of {list(scorers)}")
        score = scorers[method]

        ranked = [(c, score(user, c)) for c in self.candidates(user)]
        ranked.sort(key=lambda x: (-x[1], x[0]))
        return ranked[:k]


if __name__ == "__main__":
    g = SocialGraph()
    edges = [
        ("Alice", "Bob"), ("Alice", "Carol"), ("Alice", "David"),
        ("Bob", "Carol"), ("Bob", "Eve"),
        ("Carol", "Eve"), ("Carol", "Frank"),
        ("David", "Frank"), ("David", "Grace"),
        ("Eve", "Heidi"),
        ("Frank", "Heidi"), ("Frank", "Ivan"),
        ("Grace", "Ivan"),
    ]
    for a, b in edges:
        g.add_friendship(a, b)

    user = "Alice"
    print(f"{user}'s friends: {sorted(g.friends(user))}\n")

    for method in ("mutual", "jaccard", "adamic"):
        print(f"Recommendations ({method}):")
        for person, s in g.recommend(user, k=5, method=method):
            mutual = sorted(g.friends(user) & g.friends(person))
            print(f"  {person:<6} score={s:.3f}  mutual friends={mutual}")
        print()
