//
// Created by wy on 2021/10/25.
//
#include "master.h"
#ifndef WY_CUDA_RTREE_H
#define WY_CUDA_RTREE_H
//DATATYPE 数据类型
//ELEMTYPE 元素的类型
//NUMDIMS 维度数2/3
//TMAXNODES 阶数
//ELEMTYPEREAL 计算体积用的，你们可以直接当ELEMTYPE，不用管



//注意：我们使用节点全部用new/delete
//注意：插入和删除数据需要了解其恒定最小边界矩形。
class RTFileStream;

#define RTREE_TEMPLATE template<class DATATYPE, class ELEMTYPE, int NUMDIMS, class ELEMTYPEREAL, int TMAXNODES, int TMINNODES>
#define RTREE_QUAL RTree<DATATYPE, ELEMTYPE, NUMDIMS, ELEMTYPEREAL, TMAXNODES, TMINNODES>
template<class DATATYPE, class ELEMTYPE, int NUMDIMS,
        class ELEMTYPEREAL = ELEMTYPE, int TMAXNODES = 8, int TMINNODES = TMAXNODES / 2>
class RTree
{
protected:

    struct Node;  // 由其他内部结构和迭代器使用

public:

    enum
    {
        MAXNODES = TMAXNODES,                         //节点中最大个数
        MINNODES = TMINNODES,                         //节点中最小个数
    };


public:

    RTree();
    virtual ~RTree();

    //最大点
    //最小点
    //操作数据id 可能是零，但不允许出现负数
    void Insert(const ELEMTYPE a_min[NUMDIMS], const ELEMTYPE a_max[NUMDIMS], const DATATYPE& a_dataId); //刘


    void Remove(const ELEMTYPE a_min[NUMDIMS], const ELEMTYPE a_max[NUMDIMS], const DATATYPE& a_dataId);  //王


    //return返回找到的条目数
    //int Search(const ELEMTYPE a_min[NUMDIMS], const ELEMTYPE a_max[NUMDIMS], bool a_resultCallback(DATATYPE a_data, void* a_context), void* a_context);

    //从树中删除所有条目
    void RemoveAll();

    //统计此容器中的数据元素。这很慢，因为没有维护内部计数器。
    int Count();

    //从文件中加载树内容
    bool Load(RTFileStream& a_stream);


    //将树内容保存到文件
    bool Save(RTFileStream& a_stream);

    //迭代器
    class Iterator
    {
    private:

        enum { MAX_STACK = 32 }; //最大栈大小。几乎允许n^32，其中n是节点中的分支数

        struct StackElement
        {
            Node* m_node;
            int m_branchIndex;
        };

    public:

        Iterator()                                    { Init(); }

        ~Iterator()                                   { }

        bool IsNull()                                 { return (m_tos <= 0); }

        bool IsNotNull()                              { return (m_tos > 0); }

        //访问当前数据元素。调用方必须首先确保迭代器不为NULL。
        DATATYPE& operator*()
        {
            StackElement& curTos = m_stack[m_tos - 1];
            return curTos.m_node->m_branch[curTos.m_branchIndex].m_data;
        }


        //查找下一个数据元素
        bool operator++()                             { return FindNextData(); }

        //获取此节点的边界
        void GetBounds(ELEMTYPE a_min[NUMDIMS], ELEMTYPE a_max[NUMDIMS])
        {
            StackElement& curTos = m_stack[m_tos - 1];
            Branch& curBranch = curTos.m_node->m_branch[curTos.m_branchIndex];

            for(int index = 0; index < NUMDIMS; ++index)
            {
                a_min[index] = curBranch.m_rect.m_min[index];
                a_max[index] = curBranch.m_rect.m_max[index];
            }
        }

    private:

        void Init()                                   { m_tos = 0; }

        //查找树中的下一个数据元素（仅供内部使用）
        bool FindNextData()
        {
            for(;;)
            {
                if(m_tos <= 0)
                {
                    return false;
                }
                StackElement curTos = Pop(); //复制堆栈顶部，因为在使用时它可能会更改

                if(curTos.m_node->IsLeaf())
                {
                    //尽可能地继续浏览数据
                    if(curTos.m_branchIndex+1 < curTos.m_node->m_count)
                    {
                        //还有更多数据，请指向下一个
                        Push(curTos.m_node, curTos.m_branchIndex + 1);
                        return true;
                    }
                    //没有更多的数据，因此它将回落到以前的level
                }
                else
                {
                    if(curTos.m_branchIndex+1 < curTos.m_node->m_count)
                    {
                        Push(curTos.m_node, curTos.m_branchIndex + 1);
                    }
                    // 由于 cur 节点不是叶子，因此先Push到下一层以深入到树中
                    Node* nextLevelnode = curTos.m_node->m_branch[curTos.m_branchIndex].m_child;
                    Push(nextLevelnode, 0);

                    // 如果我们Push了一个新的叶子，当数据在Tos准备好时退出
                    if(nextLevelnode->IsLeaf())
                    {
                        return true;
                    }
                }
            }
        }

        //将节点和分支推送到迭代栈上（仅供内部使用）
        void Push(Node* a_node, int a_branchIndex)
        {
            m_stack[m_tos].m_node = a_node;
            m_stack[m_tos].m_branchIndex = a_branchIndex;
            ++m_tos;
        }

        //从迭代栈中弹出元素（仅供内部使用）
        StackElement& Pop()
        {
            --m_tos;
            return m_stack[m_tos];
        }

        StackElement m_stack[MAX_STACK];              //迭代
        int m_tos;                                    //栈顶索引

        friend RTree;
    };

    //从第一个开始迭代
    void GetFirst(Iterator& a_it)
    {
        a_it.Init();
        Node* first = m_root;
        while(first)
        {
            if(first->IsInternalNode() && first->m_count > 1)
            {
                a_it.Push(first, 1); // 深入到兄弟分支
            }
            else if(first->IsLeaf())
            {
                if(first->m_count)
                {
                    a_it.Push(first, 0);
                }
                break;
            }
            first = first->m_branch[0].m_child;
        }
    }


    void GetNext(Iterator& a_it)                    { ++a_it; }

    //迭代器是 NULL 还是结束
    bool IsNull(Iterator& a_it)                     { return a_it.IsNull(); }

    //在迭代器位置获取对象
    DATATYPE& GetAt(Iterator& a_it)                 { return *a_it; }

protected:

    ///最小边界矩形（n维）
    struct Rect
    {
        ELEMTYPE m_min[NUMDIMS];                      ///边界框的最小尺寸
        ELEMTYPE m_max[NUMDIMS];                      ///边界框的最大尺寸
    };

    /// 可以是数据，也可以是另一子树
    ///  父级决定了这一点。
    /// 如果父级为0，则这是数据
    struct Branch
    {
        Rect m_rect;                                  //Bounds
        union
        {
            Node* m_child;                              //子节点
            DATATYPE m_data;                            //Data Id or Ptr
        };
    };


    struct Node
    {
        bool IsInternalNode()                         { return (m_level > 0); }
        bool IsLeaf()                                 { return (m_level == 0); }

        int m_count;
        int m_level;
        Branch m_branch[MAXNODES];
    };

    ///删除操作后重新插入的节点链接列表
    struct ListNode
    {
        ListNode* m_next;
        Node* m_node;
    };

    ///用于查找分割分区的变量
    struct PartitionVars
    {
        int m_partition[MAXNODES+1];   //划分
        int m_total;     //总量size
        int m_minFill;    //最小填充
        int m_taken[MAXNODES+1];
        int m_count[2];     //分割的两个组里的size
        Rect m_cover[2];     //分割的两个rect
        ELEMTYPEREAL m_area[2];   //

        Branch m_branchBuf[MAXNODES+1];   //当缓存用
        int m_branchCount;      //分支当前数量
        Rect m_coverSplit;   //存缓存当前的矩阵
        ELEMTYPEREAL m_coverSplitArea;    //大小
    };

    Node* AllocNode();
    void FreeNode(Node* a_node);
    void InitNode(Node* a_node);
    void InitRect(Rect* a_rect);
    bool InsertRectRec(Rect* a_rect, const DATATYPE& a_id, Node* a_node, Node** a_newNode, int a_level);     //刘
    bool InsertRect(Rect* a_rect, const DATATYPE& a_id, Node** a_root, int a_level);                           //刘
    Rect NodeCover(Node* a_node);
    bool AddBranch(Branch* a_branch, Node* a_node, Node** a_newNode);
    void DisconnectBranch(Node* a_node, int a_index);
    int PickBranch(Rect* a_rect, Node* a_node);
    Rect CombineRect(Rect* a_rectA, Rect* a_rectB);
    void SplitNode(Node* a_node, Branch* a_branch, Node** a_newNode);
    ELEMTYPEREAL RectSphericalVolume(Rect* a_rect);
    ELEMTYPEREAL RectVolume(Rect* a_rect);
    ELEMTYPEREAL CalcRectVolume(Rect* a_rect);
    void GetBranches(Node* a_node, Branch* a_branch, PartitionVars* a_parVars);
    void ChoosePartition(PartitionVars* a_parVars, int a_minFill);
    void LoadNodes(Node* a_nodeA, Node* a_nodeB, PartitionVars* a_parVars);
    void InitParVars(PartitionVars* a_parVars, int a_maxRects, int a_minFill);
    void PickSeeds(PartitionVars* a_parVars);
    void Classify(int a_index, int a_group, PartitionVars* a_parVars);
    bool RemoveRect(Rect* a_rect, const DATATYPE& a_id, Node** a_root);                           //王
    bool RemoveRectRec(Rect* a_rect, const DATATYPE& a_id, Node* a_node, ListNode** a_listNode);                 //王
    ListNode* AllocListNode();
    void FreeListNode(ListNode* a_listNode);
    bool Overlap(Rect* a_rectA, Rect* a_rectB);
    void ReInsert(Node* a_node, ListNode** a_listNode);
    bool Search(Node* a_node, Rect* a_rect, int& a_foundCount, bool a_resultCallback(DATATYPE a_data, void* a_context), void* a_context);
    void RemoveAllRec(Node* a_node);
    void Reset();
    void CountRec(Node* a_node, int& a_count);

    bool SaveRec(Node* a_node, RTFileStream& a_stream);
    bool LoadRec(Node* a_node, RTFileStream& a_stream);

    Node* m_root;                                    //根节点
};
//合并矩阵A和B，形成大矩阵
RTREE_TEMPLATE
typename RTREE_QUAL::Rect RTREE_QUAL::CombineRect(Rect* a_rectA, Rect* a_rectB)
{
    Rect newRect;

    for(int index = 0; index < NUMDIMS; ++index)
    {
        newRect.m_min[index] = Min(a_rectA->m_min[index], a_rectB->m_min[index]);
        newRect.m_max[index] = Max(a_rectA->m_max[index], a_rectB->m_max[index]);
    }

    return newRect;
}

// 添加一个分支到一个节点。 如有必要，拆分节点。
// 如果节点没有分裂，则返回 0。 旧节点更新。
// 如果节点分裂，则返回 1，将 *new_node 设置为新节点的地址。
// 旧节点更新，成为两个之一。
RTREE_TEMPLATE
bool RTREE_QUAL::AddBranch(Branch* a_branch, Node* a_node, Node** a_newNode)
{
    if(a_node->m_count < MAXNODES)  // Split won't be necessary 不需要拆分
    {
        a_node->m_branch[a_node->m_count] = *a_branch;
        ++a_node->m_count;

        return false;
    }
    else
    {
        ASSERT(a_newNode);

        SplitNode(a_node, a_branch, a_newNode);
        return true;
    }
}

// 拆分一个节点。
// 划分节点分支和两个节点之间的额外分支。
// 旧节点是新节点之一，并且创建了一个真正的新节点。
// 尝试多种选择分区的方法，使用最佳结果。
RTREE_TEMPLATE
void RTREE_QUAL::SplitNode(Node* a_node, Branch* a_branch, Node** a_newNode)
{
    // 可以在这里使用本地，但成员或外部更快，因为它被重用
    PartitionVars localVars;
    PartitionVars* parVars = &localVars;
    int level;   //记住当前拆分节点的级别

    //将所有分支加载到缓冲区中，初始化旧节点
    level = a_node->m_level;
    GetBranches(a_node, a_branch, parVars);

    // 查找分区
    ChoosePartition(parVars, MINNODES);

    // Put branches from buffer into 2 nodes according to chosen partition
    *a_newNode = AllocNode();
    (*a_newNode)->m_level = a_node->m_level = level;
    LoadNodes(a_node, *a_newNode, parVars);

}

//将所有分支加载到缓冲区中，初始化旧节点
RTREE_TEMPLATE
void RTREE_QUAL::GetBranches(Node* a_node, Branch* a_branch, PartitionVars* a_parVars)
{

    //加载分支缓冲区
    for(int index=0; index < MAXNODES; ++index)
    {
        a_parVars->m_branchBuf[index] = a_node->m_branch[index];
    }
    a_parVars->m_branchBuf[MAXNODES] = *a_branch;
    a_parVars->m_branchCount = MAXNODES + 1;

    //计算包含集合中所有内容的矩形
    a_parVars->m_coverSplit = a_parVars->m_branchBuf[0].m_rect;
    for(int index=1; index < MAXNODES+1; ++index)  //遍历合并矩阵
    {
        a_parVars->m_coverSplit = CombineRect(&a_parVars->m_coverSplit, &a_parVars->m_branchBuf[index].m_rect);
    }
    a_parVars->m_coverSplitArea = RectVolume(&a_parVars->m_coverSplit);  //计算体积  --->给定矩形的边界球体的精确体积

    InitNode(a_node);   //初始化节点
}

//计算矩形的n维体积
RTREE_TEMPLATE
ELEMTYPEREAL RTREE_QUAL::RectVolume(Rect* a_rect)
{
    ELEMTYPEREAL volume = (ELEMTYPEREAL)1;

    for(int index=0; index<NUMDIMS; ++index)
    {
        volume *= a_rect->m_max[index] - a_rect->m_min[index];
    }


    return volume;
}

RTREE_TEMPLATE
void RTREE_QUAL::InitNode(Node* a_node)
{
    a_node->m_count = 0;
}

//实例一个节点
RTREE_TEMPLATE
typename RTREE_QUAL::Node* RTREE_QUAL::AllocNode()
{
    Node* newNode;
    newNode=new Node;
    InitNode(newNode);
    return newNode;
}

RTREE_TEMPLATE
void RTREE_QUAL::FreeNode(Node* a_node)
{
    delete a_node;
}

RTREE_TEMPLATE
void RTREE_QUAL::InitRect(Rect* a_rect)
{
    for(int index = 0; index < NUMDIMS; ++index)
    {
        a_rect->m_min[index] = (ELEMTYPE)0;
        a_rect->m_max[index] = (ELEMTYPE)0;
    }
}

//找到包含节点分支中所有矩形的最小矩形。
RTREE_TEMPLATE
typename RTREE_QUAL::Rect RTREE_QUAL::NodeCover(Node* a_node)
{
    int firstTime =true;
    Rect rect;
    InitRect(&rect);

    for (int index = 0; index < a_node->m_count; ++index)
    {
        if (firstTime)
        {
            rect=a_node->m_branch[index].m_rect;
            firstTime= false;
        }
        else
        {
            rect=CombineRect(&rect,&(a_node->m_branch[index].m_rect));
        }
    }
    return rect;
}
// 断开依赖节点。
// 当计数改变时，调用者必须返回（或停止使用迭代索引）
RTREE_TEMPLATE
void RTREE_QUAL::DisconnectBranch(Node* a_node, int a_index)
{
    //通过与最后一个元素交换来删除元素以防止数组中的间隙
    a_node->m_branch[a_index] = a_node->m_branch[a_node->m_count - 1];

    --a_node->m_count;
}

// 选择一个分支。 选择需要最小增加的那个
// 容纳新矩形的区域。 这将导致
// 当前节点中覆盖矩形的最小总面积。
// 在平局的情况下，选择之前较小的那个，得到
// 搜索时的最佳分辨率
RTREE_TEMPLATE
int RTREE_QUAL::PickBranch(Rect* a_rect, Node* a_node)
{
    bool firstTime = true;
    ELEMTYPEREAL increase;
    ELEMTYPEREAL bestIncr = (ELEMTYPEREAL)-1;
    ELEMTYPEREAL area;
    ELEMTYPEREAL bestArea;
    int best;
    Rect tempRect;

    for(int index=0; index < a_node->m_count; ++index)
    {
        Rect* curRect = &a_node->m_branch[index].m_rect;
        area = CalcRectVolume(curRect);
        tempRect = CombineRect(a_rect, curRect);
        increase = CalcRectVolume(&tempRect) - area;
        if((increase < bestIncr) || firstTime)
        {
            best = index;
            bestArea = area;
            bestIncr = increase;
            firstTime = false;
        }
        else if((increase == bestIncr) && (area < bestArea))
        {
            best = index;
            bestArea = area;
            bestIncr = increase;
        }
    }
    return best;
}

//计算矩阵大小
RTREE_TEMPLATE
ELEMTYPEREAL RTREE_QUAL::RectSphericalVolume(Rect* a_rect)
{
    ELEMTYPEREAL volume;

    volume = (ELEMTYPEREAL)a_rect->m_max[0]-(ELEMTYPEREAL)a_rect->m_min[0];
    //先算边长
    for (int index = 1; index < NUMDIMS; ++index) {
        ELEMTYPEREAL halfExtent = (ELEMTYPEREAL)a_rect->m_max[index]-(ELEMTYPEREAL)a_rect->m_min[index];
        volume*=halfExtent;
    }

    return volume;
}

//使用其中一种方法计算矩形体积
RTREE_TEMPLATE
ELEMTYPEREAL RTREE_QUAL::CalcRectVolume(Rect* a_rect)
{
    return RectVolume(a_rect);
}

///方法待定，没想好！
/* 选择分区的方法
 作为两组的点点，选择如果被单个矩形覆盖会浪费最多面积的两个矩形，
 即显然是同一组中最差的一对。剩下的，一次选择一个 被放入两组中的一组。
 选择的一组是根据哪一组在面积扩展方面具有最大差异的组 - rect 最强烈地被一组吸引而被另一组排斥。
 如果一组太满（更多会迫使其他组违反最小填充要求），则另一组得到其余的。
 最后这些是最容易进入任一组的。*/
RTREE_TEMPLATE
void RTREE_QUAL::ChoosePartition(PartitionVars* a_parVars, int a_minFill)
{
    ELEMTYPEREAL biggestDiff;
    int group,chosen,betterGroup;

    InitParVars(a_parVars,a_parVars->m_branchCount,a_minFill);  //初始化 PartitionVars 结构
    PickSeeds(a_parVars);

    while (((a_parVars->m_count[0] + a_parVars->m_count[1]) < a_parVars->m_total)
           && (a_parVars->m_count[0] < (a_parVars->m_total - a_parVars->m_minFill))
           && (a_parVars->m_count[1] < (a_parVars->m_total - a_parVars->m_minFill)))//保证总量不超过最大值
    {
        biggestDiff =(ELEMTYPEREAL)-1;
        for (int index = 0; index < a_parVars->m_total; ++index) {
            if (!a_parVars->m_taken[index])
            {
                Rect* curRect = &a_parVars->m_branchBuf[index].m_rect;
                Rect rect0 = CombineRect(curRect, &a_parVars->m_cover[0]);
                Rect rect1 = CombineRect(curRect, &a_parVars->m_cover[1]);
                ELEMTYPEREAL growth0 = CalcRectVolume(&rect0) - a_parVars->m_area[0];
                ELEMTYPEREAL growth1 = CalcRectVolume(&rect1) - a_parVars->m_area[1];
                ELEMTYPEREAL diff = growth1 - growth0;
                if(diff >= 0)
                {
                    group = 0;
                }
                else
                {
                    group = 1;
                    diff = -diff;
                }

                if(diff > biggestDiff)
                {
                    biggestDiff = diff;
                    chosen = index;
                    betterGroup = group;
                }
                else if((diff == biggestDiff) && (a_parVars->m_count[group] < a_parVars->m_count[betterGroup]))
                {
                    chosen = index;
                    betterGroup = group;
                }
            }
        }
    }
}

//根据分区将缓冲区中的分支复制到两个节点中。
RTREE_TEMPLATE
void RTREE_QUAL::LoadNodes(Node* a_nodeA, Node* a_nodeB, PartitionVars* a_parVars)
{
    for (int index = 0; index < a_parVars->m_total; ++index) {
        if (a_parVars->m_partition[index] ==0)
        {
            AddBranch(&a_parVars->m_branchBuf[index], a_nodeA);
        }
        else if (a_parVars->m_partition[index] == 1)
        {
            AddBranch(&a_parVars->m_branchBuf[index], a_nodeB);
        }
    }
}

// 初始化 PartitionVars 结构。
RTREE_TEMPLATE
void RTREE_QUAL::InitParVars(PartitionVars* a_parVars, int a_maxRects, int a_minFill)
{
    a_parVars->m_count[0] = a_parVars->m_count[1] = 0;
    a_parVars->m_area[0] = a_parVars->m_area[1] = (ELEMTYPEREAL)0;
    a_parVars->m_total = a_maxRects;
    a_parVars->m_minFill = a_minFill;
    for(int index=0; index < a_maxRects; ++index)
    {
        a_parVars->m_taken[index] = false;
        a_parVars->m_partition[index] = -1;
    }
}

//把PartitionVars中的元素分成两个点点分组
RTREE_TEMPLATE
void RTREE_QUAL::PickSeeds(PartitionVars* a_parVars)
{
    int seed0,seed1;
    ELEMTYPEREAL worst,waste;
    ELEMTYPEREAL area[MAXNODES+1];

    for (int index = 0; index < a_parVars->m_total; ++index) {
        area[index]=CalcRectVolume(&a_parVars->m_branchBuf[index].m_rect);
    }

    worst = -a_parVars->m_coverSplitArea - 1;
    for(int indexA=0; indexA < a_parVars->m_total-1; ++indexA)
    {
        for(int indexB = indexA+1; indexB < a_parVars->m_total; ++indexB)
        {
            Rect oneRect = CombineRect(&a_parVars->m_branchBuf[indexA].m_rect, &a_parVars->m_branchBuf[indexB].m_rect);
            waste = CalcRectVolume(&oneRect) - area[indexA] - area[indexB];
            if(waste > worst)
            {
                worst = waste;
                seed0 = indexA;
                seed1 = indexB;
            }
        }
    }
    Classify(seed0, 0, a_parVars);
    Classify(seed1, 1, a_parVars);
}
//在其中一个组中放置一个分支。
RTREE_TEMPLATE
void RTREE_QUAL::Classify(int a_index, int a_group, PartitionVars* a_parVars)
{
    a_parVars->m_partition[a_index] = a_group;
    a_parVars->m_taken[a_index] = true;

    if (a_parVars->m_count[a_group] == 0)
    {
        a_parVars->m_cover[a_group] = a_parVars->m_branchBuf[a_index].m_rect;
    }
    else
    {
        a_parVars->m_cover[a_group] = CombineRect(&a_parVars->m_branchBuf[a_index].m_rect, &a_parVars->m_cover[a_group]);
    }
    a_parVars->m_area[a_group] = CalcRectVolume(&a_parVars->m_cover[a_group]);
    ++a_parVars->m_count[a_group];
}
// 为 DeletRect 中使用的列表中的节点分配空间存储发生下溢的节点。
RTREE_TEMPLATE
typename RTREE_QUAL::ListNode* RTREE_QUAL::AllocListNode()
{
    return new ListNode;
}

RTREE_TEMPLATE
void RTREE_QUAL::FreeListNode(ListNode* a_listNode)
{
    delete a_listNode;
}

//确定两个矩形是否重叠。
RTREE_TEMPLATE
bool RTREE_QUAL::Overlap(Rect* a_rectA, Rect* a_rectB)
{
    for(int index=0; index < NUMDIMS; ++index)
    {
        if (a_rectA->m_min[index] > a_rectB->m_max[index] ||
            a_rectB->m_min[index] > a_rectA->m_max[index])
        {
            return false;
        }
    }
    return true;
}
//将节点添加到重新插入列表中。其所有分支机构稍后将
//可以重新插入到索引结构中。
RTREE_TEMPLATE
void RTREE_QUAL::ReInsert(Node* a_node, ListNode** a_listNode)
{
    ListNode* newListNode;

    newListNode = AllocListNode();
    newListNode->m_node = a_node;
    newListNode->m_next = *a_listNode;
    *a_listNode = newListNode;
}

RTREE_TEMPLATE
void RTREE_QUAL::RemoveAllRec(Node* a_node)
{
    if(a_node->IsInternalNode()) // This is an internal node in the tree
    {
        for(int index=0; index < a_node->m_count; ++index)
        {
            RemoveAllRec(a_node->m_branch[index].m_child);
        }
    }
    FreeNode(a_node);
}

RTREE_TEMPLATE
void RTREE_QUAL::Reset()
{
    RemoveAllRec(m_root);
}

RTREE_TEMPLATE
void RTREE_QUAL::CountRec(Node* a_node, int& a_count)
{
    if(a_node->IsInternalNode())  // not a leaf node
    {
        for(int index = 0; index < a_node->m_count; ++index)
        {
            CountRec(a_node->m_branch[index].m_child, a_count);
        }
    }
    else // A leaf node
    {
        a_count += a_node->m_count;
    }
}
template<class DATATYPE, class ELEMTYPE, int NUMDIMS, class ELEMTYPEREAL, int TMAXNODES, int TMINNODES>
bool RTree<DATATYPE, ELEMTYPE, NUMDIMS, ELEMTYPEREAL, TMAXNODES, TMINNODES>::InsertRectRec(RTree::Rect *a_rect,
                                                                                           const DATATYPE &a_id,
                                                                                           RTree::Node *a_node,
                                                                                           RTree::Node **a_newNode,
                                                                                           int a_level) {
    Branch* branch=malloc(sizeof(Branch));
    int a=PickBranch(a_rect,a_node);
    if (a_node->m_level>a_level){
        if (InsertRectRec(a_rect,a_id,a_node,a_newNode,a_level)){
            (a_node->m_branch)->rect= NodeCover(a_node);
            a_node=(a_node->m_branch[a])->m_child;
            return AddBranch(branch,a_node,a_newNode);
        } else{
            CombineRect(a_rect,(a_node->m_branch)->rect);
            return AddBranch(branch,a_node,a_newNode);
        }
    }
    if (a_node->m_level==a_level){
        return AddBranch(branch,a_node,a_newNode);
    }
    if (a_node->m_level==a_level){
        exit(EXIT_FAILURE);
    }
}

template<class DATATYPE, class ELEMTYPE, int NUMDIMS, class ELEMTYPEREAL, int TMAXNODES, int TMINNODES>
bool RTree<DATATYPE, ELEMTYPE, NUMDIMS, ELEMTYPEREAL, TMAXNODES, TMINNODES>::InsertRect(RTree::Rect *a_rect,
                                                                                        const DATATYPE &a_id,
                                                                                        RTree::Node **a_root,
                                                                                        int a_level) {
    Node* newNode=(Node*)malloc(sizeof(Node));
    Branch* branch=malloc(sizeof(Branch));
    branch->m_data=a_id;
    if(InsertRectRec(a_rect, a_id, *a_root, &newNode, a_level))  // 判断根节点是否被拆分
    {
        Node* newRoot = AllocNode();
        newNode->m_level=(*a_root)->m_level+1;
        branch->m_rect=NodeCover(*a_root);
        branch->m_child = newNode; //将分裂出来的新的根节点加入到branch
        AddBranch(&branch, newRoot, NULL);  //添加到新的根节点
        *a_root = newRoot;   //该节点为新的根节点
        return true;
    }
    return false;
}
/*int f(int x) 参数是一个int类型
int f(int &x) 参数是一个int类型的引用
区别的话，第一个当实参传过来被函数修改了，函数结束后实参还是传过来的实参
第二个当实参传过来被函数修改了，函数结束后实参是修改后的的数*/
template<class DATATYPE, class ELEMTYPE, int NUMDIMS, class ELEMTYPEREAL, int TMAXNODES, int TMINNODES>
void RTree<DATATYPE, ELEMTYPE, NUMDIMS, ELEMTYPEREAL, TMAXNODES, TMINNODES>::Insert(const ELEMTYPE *a_min,
                                                                                    const ELEMTYPE *a_max,
                                                                                    const DATATYPE &a_dataId) {
    Rect* rect=(Rect*)malloc(sizeof(Rect));
    Node** a_root=AllocNode();
    rect->m_max=*a_max;
    rect->m_min=*a_min;
    InsertRect(rect,a_dataId,a_root,0);
}
template<class DATATYPE, class ELEMTYPE, int NUMDIMS, class ELEMTYPEREAL, int TMAXNODES, int TMINNODES>
void RTree<DATATYPE, ELEMTYPE, NUMDIMS, ELEMTYPEREAL, TMAXNODES, TMINNODES>::Remove(const ELEMTYPE *a_min,
                                                                                    const ELEMTYPE *a_max,
                                                                                    const DATATYPE &a_dataId) {
    //根据min和max声明一个rect
    Rect *rect=new Rect;
    rect->m_min=*a_min;
    rect->m_max=*a_max;
    //RemoveRect(&rect, a_dataId, &m_root); 把rect和dataid还有根节点传进去
    RemoveRect(&rect,a_dataId,&m_root);
}

/*从索引结构中删除数据矩形。
传入一个指向 Rect 的指针，记录的 tid，ptr 到 ptr 到根节点。
如果没有找到记录，则返回 1，如果成功则返回 0。
RemoveRect 用于消除根*/
template<class DATATYPE, class ELEMTYPE, int NUMDIMS, class ELEMTYPEREAL, int TMAXNODES, int TMINNODES>
bool RTree<DATATYPE, ELEMTYPE, NUMDIMS, ELEMTYPEREAL, TMAXNODES, TMINNODES>::RemoveRect(RTree::Rect *a_rect,
                                                                                        const DATATYPE &a_id,
                                                                                        RTree::Node **a_root) {
    //声明一个临时节点 Node* tempNode;
    Node *tempNode=new Node;
    //声明一个节点链表，记录的是因为拆分节点之后，发生下溢，需要重新加入到树的节点 ListNode* reInsertList=NULL;
    ListNode* reInsertList=NULL;

    if(!RemoveRectRec(a_rect, a_id, *a_root, &reInsertList))   // 找到并删除了一个数据项,RemoveRectRec返回false则是删除成功
    {
        // 重新插入已消除节点的任何分支
        while(reInsertList)
        {
            tempNode = reInsertList->m_node;

            for(int index = 0; index < tempNode->m_count; ++index)
            {
                InsertRect(&(tempNode->m_branch[index].m_rect),   //这个方法是刘庆成实现的，从索引结构中添加数据矩形，你可以随便用
                           tempNode->m_branch[index].m_data,
                           a_root,
                           tempNode->m_level);
            }
            //链表指向下一个
            ListNode* remLNode = reInsertList;
            reInsertList = reInsertList->m_next;
            //当前插入过得节点可以释放了
            FreeNode(remLNode->m_node);
            FreeListNode(remLNode);
        }

        // 检查多余的根（不是叶子且只有一个孩子）并消除
        if((*a_root)->m_count == 1 && (*a_root)->IsInternalNode())
        {
            tempNode = (*a_root)->m_branch[0].m_child;
            FreeNode(*a_root);
            //删除多余的根节点之后，根节点也要发生改变
            *a_root = tempNode;
        }
        return false;
    }
    else
    {
        return true;
    }
    return false;
}

/* 从索引结构的非根部分删除一个矩形。
 由 RemoveRect 调用。 递归下降树，
 在返回的路上合并分支。
 如果没有找到记录，则返回 1，如果成功则返回 0。*/
//通俗来讲，只有被成功删除了数据项，才会返回false，否则都是true
template<class DATATYPE, class ELEMTYPE, int NUMDIMS, class ELEMTYPEREAL, int TMAXNODES, int TMINNODES>
bool RTree<DATATYPE, ELEMTYPE, NUMDIMS, ELEMTYPEREAL, TMAXNODES, TMINNODES>::RemoveRectRec(RTree::Rect *a_rect,//要删除的
                                                                                           const DATATYPE &a_id,
                                                                                           RTree::Node *a_node,//当前节点
                                                                                           RTree::ListNode **a_listNode) {

    if(a_node->IsInternalNode())   //不是叶子节点
    {
        //遍历当前节点中branch的rect
        for(int index = 0; index < a_node->m_count; ++index)
        {
            Rect* a_rectA=a_rect;
            Rect* a_rectB=a_node->m_branch[MAXNODES]->m_rect;
            //判断遍历的rect是否与我们的目标a_rect有重叠部分，用Overlap方法来看是否有重叠部分
            if (Overlap(*a_rectA, *a_rectB) == true){
                //如果有重叠部分，下一步递归判断这个子节点RemoveRectRec是否为false，如果是false，说明成功删除了一个数据项
                if(RemoveRectRec(*a_rect,&a_id,*a_node,**a_listNode) == false){
                    //删除数据项之后，判断是否发生了下溢
                    if (a_node->m_count>MAXNODES/2){
                        //没有发生下溢， 因为删除了孩子，所以只需调整父矩形的大小，用NodeCover方法来调整矩阵大小
                        NodeCover(a_node);
                    }
                    else{
                        //发生下溢， 因为孩子被移除，且节点中没有足够的条目，所以消除节点,用ReInsert方法，先将该节点中branch的孩子加到a_listNode链表
                        ReInsert(a_rect,a_listNode);
                        // (该节点就放的就是需要重新加载到索引结构中的节点)中，然后用DisconnectBranch方法把该节点从branch中去除掉
                        DisconnectBranch(a_rect,a_id);
                    }
                }
            }
        }
    }
    else     //是叶子节点
    {
        //遍历叶子节点中branch的孩子
        for(int index = 0; index < a_node->m_count; ++index){
            Node *child=a_node->m_branch[MAXNODES]->m_child;
            //如果孩子节点与目标a_id比较相等（注意把a_id强转成Node*的形式再做比较），则调用DisconnectBranch方法把该节点从branch中去除掉
            if (*child==(Node*)a_id){
                DisconnectBranch(child,a_id);
            }
        }
    }
    return false;
}



//刘庆成
/*RTREE_TEMPLATE
void RTREE_QUAL::Insert(const ELEMTYPE a_min[NUMDIMS], const ELEMTYPE a_max[NUMDIMS], const DATATYPE& a_dataId)
{
    Rect rect;
    for (int axis = 0; axis < NUMDIMS; ++axis) {
        rect.m_min[axis]=a_min[axis];
        rect.m_max[axis]=a_max[axis];
    }
    InsertRect(&rect,a_dataId,&m_root,0);
}

RTREE_TEMPLATE
bool RTREE_QUAL::InsertRect(Rect* a_rect, const DATATYPE& a_id, Node** a_root, int a_level)
{
    Node* newRoot;
    Node* newNode;
    Branch branch;

    if(InsertRectRec(a_rect, a_id, *a_root, &newNode, a_level))  // Root split
    {
        newRoot = AllocNode();  // Grow tree taller and new root
        newRoot->m_level = (*a_root)->m_level + 1;
        branch.m_rect = NodeCover(*a_root);
        branch.m_child = *a_root;
        AddBranch(&branch, newRoot, NULL);
        branch.m_rect = NodeCover(newNode);
        branch.m_child = newNode;
        AddBranch(&branch, newRoot, NULL);
        *a_root = newRoot;
        return true;
    }

    return false;
}
RTREE_TEMPLATE
bool RTREE_QUAL::InsertRectRec(Rect* a_rect, const DATATYPE& a_id, Node* a_node, Node** a_newNode, int a_level)
{
    int index;
    Branch branch;
    Node* otherNode;

    // 仍然高于插入级别，递归下树
    if(a_node->m_level > a_level)
    {
        index = PickBranch(a_rect, a_node);
        if (!InsertRectRec(a_rect, a_id, a_node->m_branch[index].m_child, &otherNode, a_level))
        {
            // Child was not split
            a_node->m_branch[index].m_rect = CombineRect(a_rect, &(a_node->m_branch[index].m_rect));
            return false;
        }
        else // Child was split
        {
            a_node->m_branch[index].m_rect = NodeCover(a_node->m_branch[index].m_child);
            branch.m_child = otherNode;
            branch.m_rect = NodeCover(otherNode);
            return AddBranch(&branch, a_node, a_newNode);
        }
    }
    else if(a_node->m_level == a_level)
    {
        branch.m_rect = *a_rect;
        branch.m_child = (Node*) a_id;
        return AddBranch(&branch, a_node, a_newNode);
    }
    else
    {
        return false;
    }
}*/

//王谷予
/*RTREE_TEMPLATE
void RTREE_QUAL::Remove(const ELEMTYPE a_min[NUMDIMS], const ELEMTYPE a_max[NUMDIMS], const DATATYPE& a_dataId)
{
    Rect rect;

    for(int axis=0; axis<NUMDIMS; ++axis)
    {
        rect.m_min[axis] = a_min[axis];
        rect.m_max[axis] = a_max[axis];
    }

    RemoveRect(&rect, a_dataId, &m_root);
}
RTREE_TEMPLATE
bool RTREE_QUAL::RemoveRect(Rect* a_rect, const DATATYPE& a_id, Node** a_root)
{

    Node* tempNode;
    ListNode* reInsertList = NULL;

    if(!RemoveRectRec(a_rect, a_id, *a_root, &reInsertList))
    {
        // 找到并删除了一个数据项
        // 重新插入已消除节点的任何分支
        while(reInsertList)
        {
            tempNode = reInsertList->m_node;

            for(int index = 0; index < tempNode->m_count; ++index)
            {
                InsertRect(&(tempNode->m_branch[index].m_rect),
                           tempNode->m_branch[index].m_data,
                           a_root,
                           tempNode->m_level);
            }

            ListNode* remLNode = reInsertList;
            reInsertList = reInsertList->m_next;

            FreeNode(remLNode->m_node);
            FreeListNode(remLNode);
        }

        if((*a_root)->m_count == 1 && (*a_root)->IsInternalNode())
        {
            tempNode = (*a_root)->m_branch[0].m_child;

            FreeNode(*a_root);
            *a_root = tempNode;
        }
        return false;
    }
    else
    {
        return true;
    }
}
RTREE_TEMPLATE
bool RTREE_QUAL::RemoveRectRec(Rect* a_rect, const DATATYPE& a_id, Node* a_node, ListNode** a_listNode)
{
    if(a_node->IsInternalNode())
    {
        for(int index = 0; index < a_node->m_count; ++index)
        {
            if(Overlap(a_rect, &(a_node->m_branch[index].m_rect)))
            {
                if(!RemoveRectRec(a_rect, a_id, a_node->m_branch[index].m_child, a_listNode))
                {
                    if(a_node->m_branch[index].m_child->m_count >= MINNODES)
                    {
                        // 删除了孩子，只需调整父矩形的大小
                        a_node->m_branch[index].m_rect = NodeCover(a_node->m_branch[index].m_child);
                    }
                    else
                    {
                        // 孩子被移除，节点中没有足够的条目，消除节点
                        ReInsert(a_node->m_branch[index].m_child, a_listNode);
                        DisconnectBranch(a_node, index); //由于计数已更改，因此必须在此调用后返回
                    }
                    return false;
                }
            }
        }
        return true;
    }
    else
    {
        for(int index = 0; index < a_node->m_count; ++index)
        {
            if(a_node->m_branch[index].m_child == (Node*)a_id)
            {
                DisconnectBranch(a_node, index);
                return false;
            }
        }
        return true;
    }
}*/
#endif //WY_CUDA_RTREE_H
