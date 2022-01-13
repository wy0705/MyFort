//
// Created by wy on 2021/10/20.
//
#include "master.h"


#ifndef WY_CUDA_LUMP_H
#define WY_CUDA_LUMP_H

//注意：我们使用block全部用new/delete
class Module
{
private:
    enum {
        NUMS=16,      //每个Block有几个Cake
        SIZE=200,     //每一块Cake的size
        MODULESIZE=10,  //每个Module有10个Block
    };

    struct Cake_y{
        int loc;
        string datas[SIZE];
    };
    struct Cake_x{
        int loc;    //当前x轴的位置
        Cake_y* m_x[SIZE];

    };
    struct Cake{
        bool IsFull() {return (m_count==40000);}

        int m_count; //有多少数据
        int n;  //标注在一个Block中的第几个Cake
        Cake_x* m_x;   //先确定x轴的值
        string m_tablename; //记录每个cake中的表名
        string m_naturename; //记录每个cake中的属性名
    };
    struct ListCake
    {
        ListCake* m_next;
        Cake* m_cake;
    };
    struct Matrix{           //矩阵---坐标
        long min[2];
        long max[2];
    };
    struct Block{
        bool IsNull() {return (m_count==0);}
        bool IsNotNull() {return (m_count>0);}

        int m_count; //已用数量
        int n; //标注是Module中的第几个Block
        Cake m_cake[NUMS];  //指向每个Cake的指针
    };


public:
    Block* m_bilokRoot;//第一个Block

    Module();
    virtual ~Module();


    class Iterator
    {
    private:

        enum { MAX_Cache = 10 };

        struct StackElement
        {
            Block* m_block;
            int m_cakeIndex;
        };

    public:

        Iterator()                                    { Init(); }

        ~Iterator()                                   { }

        bool IsNull()                                 { return (m_tos <= 0); }

        bool IsNotNull()                              { return (m_tos > 0); }

        /// 访问当前数据元素。调用方必须首先确保迭代器不为NULL。
        Cake& operator*()
        {
            StackElement& curTos = m_stack[m_tos - 1];
            return curTos.m_block->m_cake[curTos.m_cakeIndex];
        }

        /// 寻找下一个数据元素
        bool operator++()                             { return FindNextData(); }


    private:

        void Init()                                   { m_tos = 0; }

        /// Find the next data element in the tree (For internal use only)
        bool FindNextData()
        {
            for(;;)
            {
                if(m_tos <= 0)
                {
                    return false;
                }
                StackElement curTos = Pop(); //复制堆栈顶部，因为在使用时它可能会更改
                if (curTos.m_cakeIndex+1<curTos.m_block->m_count)//把当前栈顶的block中的cake全部浏览一遍
                {
                    //可以浏览该cake（待写）
                    Push(curTos.m_block,curTos.m_cakeIndex+1);
                    return true;
                }
            }
        }

        ///将节点和分支推送到迭代堆栈上（仅供内部使用）
        void Push(Block* a_block, int a_cakeIndex)
        {
            if(m_tos<MAX_Cache)
            {
                m_stack[m_tos].m_block = a_block;
                m_stack[m_tos].m_cakeIndex = a_cakeIndex;
                ++m_tos;
            } else
            {
                cout<<"栈溢出"<<endl;
            }
        }

        ///从迭代堆栈中弹出元素（仅供内部使用）
        StackElement& Pop()
        {
            if (m_tos>0)
            {
                --m_tos;
                return m_stack[m_tos];
            } else
            {
                cout<<"栈中无元素"<<endl;
            }
        }

        //遍历当前所有Block，找到Block剩余cake空间大于size的
        bool findBlockForSize(int size,Block* block){
            for (int i = m_tos; i > 0 ; --i) {
                if ((16-m_stack[i].m_block->m_count)>size)//说明该block有剩余空间
                {
                    StackElement& curTos = m_stack[i];
                    block=curTos.m_block;
                    return true;
                }
            }
            //该block没有剩余空间
            return false;
        }

        //遍历当前所有block中的cake，寻找有没有表名Tablename
        bool findBlockForTablename(string Tablename,Block* block){
            for (int i = m_tos; i > 0; --i) {
                StackElement curTos = m_stack[i];
                for (int j = curTos.m_block->m_count; j > 0; --j) {
                    if (curTos.m_block->m_cake->m_tablename==Tablename)
                    {
                        block=curTos.m_block;
                        return true;
                    }
                }
            }
            //整个模块中没有Tablename
            return false;
        }

        StackElement m_stack[MAX_Cache];              ///迭代
        int m_tos;                                    ///栈顶索引

        friend Module; // Allow hiding of non-public functions while allowing manipulation by logical owner
    };


    Block* AllocBlock();
    bool AddBlock(Iterator& iterator,Block* block);
    Matrix* insertTable(Iterator& iterator,string Tablename,Nature* natures);    //建表
    int deleteTable(Iterator& iterator,Matrix* location,string Tablename);         //删表
    //int updateTablename(string oldTablename,string newTablename);      //改表名
    Matrix* updateForNature(Matrix* location,string Tablename,Nature* natures);    //改列
    bool AddCake(Block* m_block,Cake* m_cake);   //向block中添加cake
    bool InitCake(Cake* m_cake);
    bool InitBlock(Block* m_block);
    bool TableToListCake(Block* m_block,string m_ablename,ListCake* m_listcake);  //将block中的tablename读到listcake中
    bool ComparedNature(ListCake* m_listcake,Nature* m_nature,Nature* differnature);  //true为添加属性，false为删除属性，differnature放的是差异属性
    int ToMatrix(ListCake* m_listCake,Matrix* m_matrix);//计算位置

};

//实例一个block
Module::Block* Module::AllocBlock()
{
    Block* newBlock;
    newBlock=new Block;
    InitBlock(newBlock);
    return newBlock;
}

bool Module::InitBlock(Block *m_block)
{
    m_block->m_count=0;
}
bool Module::AddBlock(Iterator& iterator,Block *block){
    iterator.Push()
}

Module::Matrix* Module::insertTable(Iterator& iterator,string Tablename, Nature *natures) {
    Matrix* newMatrix;
    newMatrix=new Matrix;
    Block* top=iterator->m_stack[iterator->m_tos].m_block;
    //属性个数
    int count=0;
    while (true) {
        if(natures[count].naturename.empty()){
            break;
        }
        count++;
    }
    int m_c=top->m_count;
    int num;

    if ((16-m_c)%4==0){
        num=((16-m_c)/4)*4;
    } else {
        num=((16-m_c)/4+1)*4;
    }
    //如果够用的话
    if (num >= count){
        int i;
        for (i = 0; i <count ; ++i) {
            Cake cake=top->m_cake[num+i+1];
            cake.m_tablename=Tablename;
            cake.n=num+i+1;
            cake.m_count=0;
            cake.m_naturename=natures[i].naturename;
        }
        newMatrix->min[0]=num/4*200;
        newMatrix->min[1]=0;
        newMatrix->max[0]=(num+i+1)%4*200;
        if (newMatrix->max[0]==0){
            newMatrix->max[1]=(num+i+1)/4*200;
        } else {
            newMatrix->max[1]=((num+i+1)/4+1)*200;
        }

    } else {
        Block* newBlock;
        newBlock=AllocBlock();
        int a=iterator->m_tos;
        newBlock->n=a;
        int j;
        for ( j = 0; j <count ; ++j) {
            Cake cake=newBlock->m_cake[j];
            cake.m_tablename=Tablename;
            cake.n=j;
            cake.m_count=0;
            cake.m_naturename=natures[j].naturename;
        }
        iterator->Push(newBlock,a);
        newMatrix->min[0]=0;
        newMatrix->min[1]=0;
        newMatrix->max[0]=j%4*200;
        if (newMatrix->max[0]==0){
            newMatrix->max[1]=j/4*200;
        } else {
            newMatrix->max[1]=(j/4+1)*200;
        }
    }
    return newMatrix;
}

int Module::deleteTable(Iterator& iterator,Module::Matrix *location, string Tablename) {
    Block* deletingBlock;
    int begin=location->min[0]+location->min[1]*4;
    int last=location->max[0]+location->max[1]*4;
    int i,j;
    int flag=0;
    //删除cake
    //找到位置
    for ( i = 0; i < 10; ++i) {
        deletingBlock=iterator->m_stack[i].m_block;
        for ( j = 0; j < 16; ++j) {
            if (deletingBlock->m_cake[j].m_tablename==Tablename){
                deletingBlock->m_cake[j].m_tablename=="xxx";
                flag=9;
                break;
            }
        }
        if (flag==9){
            break;
        }
    }
    //是否决定删除block,pop
    deletingBlock=iterator->m_stack[i].m_block;
    for (int k = 0; k <16 ; ++k) {
        if (!(deletingBlock->m_cake->m_tablename.compare("xxx")||deletingBlock->m_cake->m_tablename.empty())){
            flag=99;
        }
    }
    if (flag==99){
        iterator->Pop();
    }
    return last-begin;
}
Matrix Module::updateForNature(Matrix *location, string Tablename, Nature *natures) {//改列

    ListCake *m_listcake = nullptr;

    for(auto & i : m_bilokRoot->m_cake)
    {
        m_listcake = reinterpret_cast<ListCake *>(TableToListCake(m_bilokRoot, i.m_tablename, m_listcake));//将block中的所有tablename读到listcake中
    }
    //1 计算位置
    int **m_toMatrix=ToMatrix(m_listcake,location);
    location->min[0]=m_toMatrix[0][0];location->min[1]=m_toMatrix[0][1];
    location->max[0]=m_toMatrix[1][0];location->max[2]=m_toMatrix[1][1];

    //2 //遍历当前所有block中的cake，寻找有没有表名Tablename
    Iterator iterator;
    if(iterator.findBlockForTablename(Tablename, m_bilokRoot)){//有表名Tablename
        //3 //将block中的tablename读到listcake中
        m_listcake = reinterpret_cast<ListCake *>(TableToListCake(m_bilokRoot, Tablename, m_listcake));
        Nature *differnature;
        //true为添加属性，false为删除属性，differnature放的是差异属性
        if (ComparedNature(m_listcake, natures, differnature))//添加属性
        {
            int size=m_listcake->m_cake->m_count - m_bilokRoot->m_count;//增量
            //遍历当前所有Block，找到Block剩余cake空间大于size的
            if(iterator.findBlockForSize(size, m_bilokRoot))//有剩余空间
            {
                AddCake(m_bilokRoot,m_bilokRoot->m_cake);   //向block中添加cake
            }else//没有剩余空间
            {
                if(size>16){
                    cout<<"添加失败，没有剩余空间"<<endl;
                }else
                {
                    AddBlock(m_bilokRoot);
                    ListCake *m_listcake1 = nullptr;
                    for(auto & i : m_bilokRoot->m_cake)
                    {
                        TableToListCake(m_bilokRoot,i.m_tablename,m_listcake1);//将新的block中的所有tablename读到listcake中
                    }
                    //计算新的位置
                    m_toMatrix=ToMatrix(m_listcake1,location);
                    location->min[0]=m_toMatrix[0][0];location->min[1]=m_toMatrix[0][1];
                    location->max[0]=m_toMatrix[1][0];location->max[2]=m_toMatrix[1][1];
                }
            }
        }else//删除属性
        {
            //初始化
            InitCake(m_bilokRoot->m_cake);
            //调用RTree
            // (颖姐说先别管)
        }
    }
    return location;
}
#endif //WY_CUDA_LUMP_H
