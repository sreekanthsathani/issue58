#include <string>
#include <vector>

class TreeNode;

class TreeDiff
{
public:
	static std::vector<size_t> diffTrees(const std::string &t1, const std::string &t2, bool &error,
		std::vector<size_t> *deleted_ids, std::vector<size_t>* large_unchanged_subtrees,
		std::vector<size_t> *modified_inplace_ids, std::vector<size_t> &dir_diffs,
		std::vector<size_t> *deleted_inplace_ids, bool has_symbit, bool is_windows);

private:
	static void gatherDiffs(TreeNode *t1, TreeNode *t2, size_t depth, std::vector<size_t> &diffs,
		std::vector<size_t> *modified_inplace_ids, std::vector<size_t> &dir_diffs,
		std::vector<size_t> *deleted_inplace_ids, bool has_symbit, bool is_window);
	static void gatherDeletes(TreeNode *t1, std::vector<size_t> &deleted_ids);
	static void gatherLargeUnchangedSubtrees(TreeNode *t2, std::vector<size_t> &changed_subtrees);
	static void subtreeChanged(TreeNode* t2);
	static void subtreeChangedParent(TreeNode* p);
	static size_t getTreesize(TreeNode* t, size_t limit);
	static bool isSymlink(TreeNode* n, bool has_symbit, bool is_window);
};