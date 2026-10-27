#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

class TreeError : public runtime_error {
public:
    explicit TreeError(const string& message) : runtime_error(message) {}
};

template <typename T>
class Tree {
public:
    struct Node {
        T value;
        Node* parent;
        vector<unique_ptr<Node>> children;

        explicit Node(T item, Node* parentNode = nullptr)
            : value(move(item)), parent(parentNode) {
        }
    };

    Node* root() const {
        return root_.get();
    }

    Node* createRoot(T value) {
        if (root_) {
            throw TreeError("Root already exists");
        }
        root_ = make_unique<Node>(move(value));
        return root_.get();
    }

    Node* addChild(Node* parent, T value) {
        if (!parent) {
            throw TreeError("Parent does not exist");
        }
        parent->children.push_back(make_unique<Node>(move(value), parent));
        return parent->children.back().get();
    }

    vector<Node*> childrenOf(Node* node) const {
        if (!node) {
            throw TreeError("Node does not exist");
        }
        vector<Node*> result;
        result.reserve(node->children.size());
        for (const auto& child : node->children) {
            result.push_back(child.get());
        }
        return result;
    }

    vector<Node*> descendantsOf(Node* node) const {
        if (!node) {
            throw TreeError("Node does not exist");
        }
        vector<Node*> result;
        for (const auto& child : node->children) {
            collect(child.get(), result);
        }
        return result;
    }

    vector<Node*> pathFromRoot(Node* node) const {
        if (!node) {
            throw TreeError("Node does not exist");
        }
        vector<Node*> result;
        for (Node* current = node; current != nullptr; current = current->parent) {
            result.push_back(current);
        }
        reverse(result.begin(), result.end());
        return result;
    }

    Node* lowestCommonAncestor(Node* first, Node* second) const {
        if (!first || !second) {
            throw TreeError("Node does not exist");
        }
        vector<Node*> firstPath = pathFromRoot(first);
        vector<Node*> secondPath = pathFromRoot(second);
        Node* common = nullptr;
        size_t i = 0;
        while (i < firstPath.size() && i < secondPath.size() &&
            firstPath[i] == secondPath[i]) {
            common = firstPath[i];
            ++i;
        }
        if (!common) {
            throw TreeError("Nodes do not belong to the same tree");
        }
        return common;
    }

    void removeLeaf(Node* node) {
        if (!node) {
            throw TreeError("Node does not exist");
        }
        if (node == root_.get()) {
            throw TreeError("Cannot remove the root");
        }
        if (!node->children.empty()) {
            throw TreeError("Cannot remove a node with children");
        }
        Node* parent = node->parent;
        auto it = find_if(parent->children.begin(), parent->children.end(),
            [node](const unique_ptr<Node>& child) {
                return child.get() == node;
            });
        if (it == parent->children.end()) {
            throw TreeError("Node is not attached to its parent");
        }
        parent->children.erase(it);
    }

    void print(int indent = 0) const {
        if (root_) {
            printImpl(root_.get(), indent);
        }
    }

private:
    unique_ptr<Node> root_;

    static void collect(Node* node, vector<Node*>& result) {
        result.push_back(node);
        for (const auto& child : node->children) {
            collect(child.get(), result);
        }
    }

    void printImpl(Node* node, int indent) {
        string pad(indent, ' ');

        cout << pad
             << "version " << node->value.id
             << ": " << node->value.content << '\n';

        for (const auto& child : node->children) {
            printImpl(child.get(), indent + 2);
        }
    }
};

struct VersionChange {
    size_t position = 0;
    string removed;
    string inserted;
};

struct Version {
    int id;
    string initialContent;
    VersionChange change;
    bool isInitial;
};

class VersionHistory {
public:
    using Node = Tree<Version>::Node;

    Node* createInitialVersion(int id, string content) {
        if (tree_.root()) {
            throw TreeError("Initial version already exists");
        }
        ensureUniqueId(id);
        Node* node = tree_.createRoot(Version{ id, move(content), VersionChange{}, true });
        byId_.emplace(id, node);
        return node;
    }

    Node* createVersion(int parentId, int id, size_t position,
        string removed, string inserted) {
        ensureUniqueId(id);
        Node* parent = requireNode(parentId);
        string parentContent = contentOf(parentId);
        if (position > parentContent.size()) {
            throw TreeError("Change position is outside the document");
        }
        if (position + removed.size() > parentContent.size()) {
            throw TreeError("Removed text extends beyond the document");
        }
        if (parentContent.compare(position, removed.size(), removed) != 0) {
            throw TreeError("Removed text does not match the document at the given position");
        }
        Version version{ id, "", VersionChange{position, move(removed), move(inserted)}, false };
        Node* node = tree_.addChild(parent, move(version));
        byId_.emplace(id, node);
        return node;
    }

    Node* findVersion(int id) const {
        auto it = byId_.find(id);
        return it == byId_.end() ? nullptr : it->second;
    }

    string contentOf(int id) const {
        return reconstruct(requireNode(id));
    }

    vector<int> directChildren(int id) const {
        vector<int> result;
        for (Node* child : tree_.childrenOf(requireNode(id))) {
            result.push_back(child->value.id);
        }
        return result;
    }

    vector<int> allDescendants(int id) const {
        vector<int> result;
        for (Node* descendant : tree_.descendantsOf(requireNode(id))) {
            result.push_back(descendant->value.id);
        }
        return result;
    }

    vector<int> pathFromRoot(int id) const {
        vector<int> result;
        for (Node* node : tree_.pathFromRoot(requireNode(id))) {
            result.push_back(node->value.id);
        }
        return result;
    }

    int commonAncestor(int firstId, int secondId) const {
        return tree_.lowestCommonAncestor(requireNode(firstId), requireNode(secondId))->value.id;
    }

    void removeVersion(int id) {
        Node* node = requireNode(id);
        if (node == tree_.root()) {
            throw TreeError("Cannot remove the initial version");
        }
        if (!node->children.empty()) {
            throw TreeError("Cannot remove a version with descendants");
        }
        tree_.removeLeaf(node);
        byId_.erase(id);
    }

    void print() const {
        tree_.print();
    }

private:
    Tree<Version> tree_;
    unordered_map<int, Node*> byId_;

    void ensureUniqueId(int id) const {
        if (byId_.find(id) != byId_.end()) {
            throw TreeError("Version ID already exists: " + to_string(id));
        }
    }

    Node* requireNode(int id) const {
        Node* node = findVersion(id);
        if (!node) {
            throw TreeError("Version not found: " + to_string(id));
        }
        return node;
    }

    string reconstruct(const Node* node) const {
        if (node->value.isInitial) {
            return node->value.initialContent;
        }
        string content = reconstruct(node->parent);
        const VersionChange& change = node->value.change;
        content.replace(change.position, change.removed.size(), change.inserted);
        return content;
    }
};

template <typename T>
void printValues(const string& title, const vector<T>& values) {
    cout << title;
    if (values.empty()) {
        cout << "none";
    }
    else {
        for (size_t i = 0; i < values.size(); ++i) {
            if (i != 0) {
                cout << " -> ";
            }
            cout << values[i];
        }
    }
    cout << '\n';
}

int main() {
    try {
        VersionHistory history;
        history.createInitialVersion(1, "Hello world");
        history.createVersion(1, 2, 6, "world", "student");
        history.createVersion(2, 3, 0, "Hello", "Hi");
        history.createVersion(2, 4, 13, "", "!");
        history.createVersion(4, 5, 0, "Hello", "Welcome");
        history.createVersion(1, 6, 6, "world", "everyone");
        history.createVersion(6, 7, 0, "Hello", "Good morning");

        cout << "Version history and stored changes:\n";
        history.print();

        cout << "\nReconstructed content of version 3: " << history.contentOf(3) << '\n';
        cout << "Reconstructed content of version 5: " << history.contentOf(5) << '\n';
        cout << "Reconstructed content of version 7: " << history.contentOf(7) << '\n';

        printValues("Direct children of version 2: ", history.directChildren(2));
        printValues("All descendants of version 2: ", history.allDescendants(2));
        printValues("Path to version 5: ", history.pathFromRoot(5));
        cout << "Common ancestor of versions 3 and 5: " << history.commonAncestor(3, 5) << '\n';
        cout << "Common ancestor of versions 3 and 7: " << history.commonAncestor(3, 7) << '\n';

        history.removeVersion(5);

        bool duplicateRejected = false;
        try {
            history.createVersion(2, 4, 0, "", "duplicate");
        }
        catch (const TreeError&) {
            duplicateRejected = true;
        }

        bool wrongRemovedTextRejected = false;
        try {
            history.createVersion(2, 8, 0, "Wrong", "text");
        }
        catch (const TreeError&) {
            wrongRemovedTextRejected = true;
        }

        bool nonLeafRejected = false;
        try {
            history.removeVersion(2);
        }
        catch (const TreeError&) {
            nonLeafRejected = true;
        }

        bool missingParentRejected = false;
        try {
            history.createVersion(99, 9, 0, "", "invalid");
        }
        catch (const TreeError&) {
            missingParentRejected = true;
        }

        cout << "\nDuplicate ID test: " << (duplicateRejected ? "OK" : "FAILED") << '\n';
        cout << "Wrong removed text test: " << (wrongRemovedTextRejected ? "OK" : "FAILED") << '\n';
        cout << "Non-leaf removal test: " << (nonLeafRejected ? "OK" : "FAILED") << '\n';
        cout << "Missing parent test: " << (missingParentRejected ? "OK" : "FAILED") << '\n';

        return duplicateRejected && wrongRemovedTextRejected && nonLeafRejected &&
            missingParentRejected
            ? 0
            : 1;
    }
    catch (const exception& error) {
        cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
