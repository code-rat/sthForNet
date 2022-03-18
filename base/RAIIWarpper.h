#ifndef _RAII_WARPPER_H_
#define _RAII_WARPPER_H_

template<class T>
class RAIIWarpper
{
private:
    T *ptr;
public:
    class RAIIWapper(T *p):ptr(p){}

    virtual ~RAIIWapper(){
        if(ptr!=NULL){
            delete ptr;
            ptr=NULL;
        }
    }
  
};


#endif