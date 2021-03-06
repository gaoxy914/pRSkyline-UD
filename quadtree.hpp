#ifndef __QUADTREE__
#define __QUADTREE__

#include "object.hpp"

class QuadTree {
    int m;
    int c;
    int n_child;

    struct Info {
        int *sigma;
        double beta;
        int xi;

        Info() : sigma(nullptr), beta(1), xi(0) {}
    };

    /*
     * internal node :
     * pointers to children
     * leaf node :
     * 1) planes through its region
     * 2) tree of planes above its region or info
     */
    struct Node {
        vector<Node*> children;
        int n_through;
        int* through_planes; // planes through the leaf
        void* tree_or_info;

        Node() : n_through(0), through_planes(nullptr), tree_or_info(nullptr) {}
    };

    Node *root;
    vector<HyperPlane> planes;
    HyperBox space;

    /*
     * above (formal parameter) : record planes above the space
     * depth : tree height
     * level : indicator for tree or info
     */
    Node* BuildRecursive(const HyperBox& space, vector<int> above, const vector<int>& through, const int& depth, const int& level) {
        Node *node = new Node();
        if (through.size() <= c) {
            node->n_through = through.size();
            node->through_planes = new int[through.size()];
            for (int i = 0; i < through.size(); ++ i) node->through_planes[i] = through[i];
            if (level == 0) { // build information
                Info info;
                info.sigma = new int[m];
                memset(info.sigma, 0, m*sizeof(info.sigma));
                for (int p : above) {
                    if (info.sigma[planes[p].obj_id] + 1 == planes[p].prob) {
                        info.beta *= planes[p].prob;
                        info.xi += 1;
                        info.sigma[planes[p].obj_id] += 1;
                    } else {
                        double delta = planes[p].prob - info.sigma[planes[p].obj_id];
                        info.beta *= (delta - 1)/delta;
                        info.sigma[planes[p].obj_id] += 1;
                    }
                }
                node->tree_or_info = &info;
            } else { // build next level tree
                node->tree_or_info = BuildRecursive(this->space, vector<int>{}, above, 1, level - 1);
            }
        } else {
            node->children.reserve(n_child);
            for (int i = 0; i < n_child; ++ i) {
                HyperBox subspace = space.GetSubSpace(i);
                vector<int> subthrough;
                int n_ins_above = 0;
                for (int p : through) {
                    if (planes[p].Above(subspace)) { above.push_back(p); n_ins_above ++; }
                    else if (planes[p].Intersect(subspace)) subthrough.push_back(p);
                }
                node->children[i] = BuildRecursive(subspace, above, subthrough, depth + 1, level);
                for (int i = 0; i < n_ins_above; ++ i) above.pop_back();
            }
        }
        return node;
    }

    void ClearRecursive(Node *node) {
        if (node == nullptr) return;
        for (int i = 0; i < node->children.size(); ++ i) ClearRecursive(node->children[i]);
        delete node;
    }

    double QueryRecursive(const Node* node, const HyperBox& space, vector<int>& through, const vector<double*>& points, const int& level, \
        const HyperBox& R, const HyperPlane& plane) const {
        if (node->children.empty()) { // leaf node
            if (level == 0) { // cal prob with info and planes in though
                Info *info = static_cast<Info*>(node->tree_or_info);
                if (info->xi > 1 || (info->xi == 1 && info->sigma[plane.obj_id] != plane.prob)) return 0;
                unordered_map<int, int> n_obj;
                unordered_map<int, double> prob_obj;
                for (int p : through) {
                    if (planes[p].obj_id == plane.obj_id) continue;
                    if (planes[p].RDominates(plane, R)) {
                        if (n_obj.find(planes[p].obj_id) != n_obj.end()) ++ n_obj[planes[p].obj_id];
                        else { n_obj[planes[p].obj_id] = 1; prob_obj[planes[p].prob] = planes[p].prob; }
                    }
                }
                double beta = info->beta*plane.prob/info->sigma[plane.obj_id];
                for (auto iter : n_obj) {
                    if (info->sigma[iter.first] + iter.second == prob_obj[iter.first]) return 0;
                    else {
                        double delta = prob_obj[iter.first] - info->sigma[iter.first];
                        beta *= (delta - iter.second)/delta;
                    }
                }
                return beta/plane.prob;
            } else {
                for (int i = 0; i < node->n_through; ++ i) through.push_back(node->through_planes[i]);
                QueryRecursive(static_cast<Node*>(node->tree_or_info), this->space, through, points, level - 1, R, plane);
            }
        } else {
            HyperBox subspace(space.dim);
            int k = space.PointLocation(points[level], subspace);
            QueryRecursive(node->children[k], subspace, through, points, level, R, plane);
        }
    }

public:
    QuadTree() : m(0), root(nullptr), n_child(pow(2, Dim)) {}

    QuadTree(const int& m, const vector<HyperPlane>& planes, const HyperBox& space) {
        this->m = m;
        n_child = pow(2, Dim);
        root = nullptr;
        Build(planes, space);
    }

    ~QuadTree() {
        Clear();
    }

    void Clear() {
        ClearRecursive(root);
        root = nullptr;
    }

    void Build(const vector<HyperPlane>& planes, const HyperBox& space) {
        if (root != nullptr) Clear();
        this->planes = planes;
        this->space = space;
        vector<int> above;
        vector<int> through;
        for (int i = 0; i < planes.size(); ++ i) {
            if (planes[i].Above(space)) above.push_back(i);
            else if (planes[i].Intersect(space)) through.push_back(i);
        }
        root = BuildRecursive(space, above, through, 1, pow(2, Dim - 1) - 1);
    }

    map<int, double> CalProb(const HyperBox& R) const {
        map<int, double> results;
        for (auto p : planes) {
            // if p skyline prob = 0 continue
            vector<int> through;
            vector<double*> points(pow(2, Dim - 1));
            p.GetQueryPoints(R, points);
            results[p.ins_id] = QueryRecursive(root, space, through, points, pow(2, Dim - 1) - 1, R, p);
        }
    }

};

#endif