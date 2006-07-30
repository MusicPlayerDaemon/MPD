#define CHILDREN_PER_NODE 3
#define DATA_PER_NODE 2

typedef struct _TreeNode
{
	void * data[DATA_PER_NODE];
	struct _TreeNode * parent;
	struct _TreeNode * children[CHILDREN_PER_NODE];
	unsigned dataCount;
} TreeNode;

typedef int (*TreeCompareDataFunction)(void * data1, void * data2);
typedef void (*TreeFreeDataFunction)(void * data);

typedef struct _Tree
{
	TreeCompareDataFunction compareData;
	TreeFreeDataFunction freeData;
	TreeNode * rootNode;
} Tree;

typedef struct _TreeIterator
{
	Tree * tree;
	TreeNode * node;
	unsigned which;
} TreeIterator;

Tree * MakeTree(TreeCompareDataFunction compareFunc, 
		TreeFreeDataFunction freeData);

void SetIteratorToBegin(TreeIterator * iter, Tree * tree);

int InsertIntoTree(Tree * tree, void * data);

void DeleteFromTree(Tree * tree, void * data);
