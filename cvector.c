#include "cvector.h" 

#ifndef __gnu_linux__ 
#define __func__ "unknown" 
#define inline __forceinline 
#endif 

#define CWARNING_ITER(cv, iter, file, func, line)  do { \
   if ((cvector_begin(cv) > iter) || (cvector_end(cv) <= iter)) { \
                fprintf(stderr, "var(" #iter ") warng out of range, "\
                    "at file:%s func:%s line:%d!!\n", file, func, line); \
                return CVEFAILED; \
    }\
} while (0)

struct _cvector
{
    void* cv_pdata;
    size_t cv_len;
    size_t cv_tot_len;
    size_t cv_size;
};

cvector cvector_create(const size_t size)
{
    cvector cv = (cvector)malloc(sizeof(struct _cvector));

    if (cv == 0) return 0;

    cv->cv_pdata = malloc(MIN_LEN * size);

    if (!cv->cv_pdata)
    {
        free(cv);
        return 0;
    }

    cv->cv_size = size;
    cv->cv_tot_len = MIN_LEN;
    cv->cv_len = 0;

    return cv;
}

void cvector_destroy(const cvector cv)
{
    if (cv != 0) {
        free(cv->cv_pdata);
        free(cv);
    }
}

size_t cvector_length(const cvector cv)
{
    return cv!=0 ? cv->cv_len : 0;
}

int cvector_pushback(const cvector cv, void* memb)
{
    if (cv != 0) {
        if (cv->cv_len >= cv->cv_tot_len)
        {
            void* pd_sav = cv->cv_pdata;
            cv->cv_tot_len <<= EXPANED_VAL;
            cv->cv_pdata = realloc(cv->cv_pdata, cv->cv_tot_len * cv->cv_size);

            if (!cv->cv_pdata)
            {
                cv->cv_pdata = pd_sav;
                cv->cv_tot_len >>= EXPANED_VAL;
                return CVEPUSHBACK;
            }
        }

        memcpy((char*)cv->cv_pdata + cv->cv_len * cv->cv_size, memb, cv->cv_size);
        cv->cv_len++;
    }
    return CVESUCCESS;
}

int cvector_popback(const cvector cv, void* memb)
{
    if (cv != 0) {
        if (cv->cv_len <= 0) return CVEPOPBACK;

        cv->cv_len--;
        memcpy(memb, (char*)cv->cv_pdata + cv->cv_len * cv->cv_size, cv->cv_size);

        if ((cv->cv_tot_len >= (MIN_LEN << REDUSED_VAL))
            && (cv->cv_len <= (cv->cv_tot_len >> REDUSED_VAL)))
        {
            void* pd_sav = cv->cv_pdata;
            cv->cv_tot_len >>= EXPANED_VAL;
            cv->cv_pdata = realloc(cv->cv_pdata, cv->cv_tot_len * cv->cv_size);

            if (!cv->cv_pdata)
            {
                cv->cv_tot_len <<= EXPANED_VAL;
                cv->cv_pdata = pd_sav;
                return CVEPOPBACK;
            }
        }
    }
    return CVESUCCESS;
}

size_t cvector_iter_at(const cvector cv, citerator iter)
{
    if (cv != 0) {
        CWARNING_ITER(cv, iter, __FILE__, __func__, __LINE__);
        return ((char*)iter - (char*)cv->cv_pdata) / cv->cv_size;
    }
    return 0;
}

int cvector_iter_val(const cvector cv, citerator iter, void* memb)
{
    if (cv != 0) {
        CWARNING_ITER(cv, iter, __FILE__, __func__, __LINE__);
        memcpy(memb, iter, cv->cv_size);
        return 0;
    }
}

citerator cvector_begin(const cvector cv)
{
    return cv!=0 ? cv->cv_pdata:0;
}

citerator cvector_end(const cvector cv)
{
    return cv!=0? (char*)cv->cv_pdata + (cv->cv_size * cv->cv_len):0;
}

static inline void cvmemove_foreward(const cvector cv, void* from, void* to)
{
    if (cv != 0) {
        size_t size = cv->cv_size;
        char* p;
        for (p = (char*)to; p >= (char*)from; p -= size) memcpy(p + size, p, size);
    }
}

static inline void cvmemove_backward(const cvector cv, void* from, void* to)
{
    if (cv != 0) {
        memcpy(from, (char*)from + cv->cv_size, (char*)to - (char*)from);
    }
}

int cvector_insert(const cvector cv, citerator iter, void* memb)
{
    if (cv != 0) {
        CWARNING_ITER(cv, iter, __FILE__, __func__, __LINE__);

        if (cv->cv_len >= cv->cv_tot_len)
        {
            void* pd_sav = cv->cv_pdata;
            cv->cv_tot_len <<= EXPANED_VAL;
            cv->cv_pdata = realloc(cv->cv_pdata, cv->cv_tot_len * cv->cv_size);

            if (!cv->cv_pdata)
            {
                cv->cv_pdata = pd_sav;
                cv->cv_tot_len >>= EXPANED_VAL;
                return CVEINSERT;
            }
        }

        cvmemove_foreward(cv, iter, (char*)cv->cv_pdata + cv->cv_len * cv->cv_size);
        memcpy(iter, memb, cv->cv_size);
        cv->cv_len++;
    }
    return CVESUCCESS;
}

int cvector_insert_at(const cvector cv, size_t index, void* memb)
{
    if (cv != 0) {
        citerator iter = 0;

        if (index >= cv->cv_tot_len)
        {
            cv->cv_len = index + 1;
            while (cv->cv_len >= cv->cv_tot_len) cv->cv_tot_len <<= EXPANED_VAL;
            cv->cv_pdata = realloc(cv->cv_pdata, cv->cv_tot_len * cv->cv_size);
            if (cv->cv_pdata != 0) {
                iter = (char*)cv->cv_pdata + cv->cv_size * index;
                memcpy(iter, memb, cv->cv_size);
            }
        }
        else
        {
            iter = (char*)cv->cv_pdata + cv->cv_size * index;
            cvector_insert(cv, iter, memb);
        }
    }
    return 0;
}

citerator cvector_next(const cvector cv, citerator iter)
{
    return cv!=0 ? (char*)iter + cv->cv_size : 0;
}

int cvector_val(const cvector cv, citerator iter, void* memb)
{
    if (cv != 0) {
        memcpy(memb, iter, cv->cv_size);
    }
    return 0;
}

int cvector_val_at(const cvector cv, size_t index, void* memb)
{
    if (cv != 0) {
        memcpy(memb, (char*)cv->cv_pdata + index * cv->cv_size, cv->cv_size);
    }
    return 0;
}

int cvector_rm(const cvector cv, citerator iter)
{
    if (cv != 0) {
        citerator from;
        citerator end;
        CWARNING_ITER(cv, iter, __FILE__, __func__, __LINE__);
        from = iter;
        end = cvector_end(cv);
        memcpy(from, (char*)from + cv->cv_size, (char*)end - (char*)from);
        cv->cv_len--;

        if ((cv->cv_tot_len >= (MIN_LEN << REDUSED_VAL))
            && (cv->cv_len <= (cv->cv_tot_len >> REDUSED_VAL)))
        {
            void* pd_sav = cv->cv_pdata;
            cv->cv_tot_len >>= EXPANED_VAL;
            cv->cv_pdata = realloc(cv->cv_pdata, cv->cv_tot_len * cv->cv_size);

            if (!cv->cv_pdata)
            {
                cv->cv_tot_len <<= EXPANED_VAL;
                cv->cv_pdata = pd_sav;
                return CVERM;
            }
        }
    }
    return CVESUCCESS;
}

int cvector_rm_at(const cvector cv, size_t index)
{
    if (cv != 0) {
        citerator iter;
        iter = (char*)cv->cv_pdata + cv->cv_size * index;
        CWARNING_ITER(cv, iter, __FILE__, __func__, __LINE__);
        return cvector_rm(cv, iter);
    }
    return 0;
}

void cv_info(const cvector cv)
{
    if (cv != 0) {
        printf("\n\ntot :%s : %d\n", __func__, (int)cv->cv_tot_len);
        printf("len :%s : %d\n", __func__, (int)cv->cv_len);
        printf("size:%s : %d\n\n", __func__, (int)cv->cv_size);
    }
}

void cv_print(const cvector cv)
{
    if (cv != 0) {
        int num;
        citerator iter;

        if (cvector_length(cv) == 0)
            fprintf(stderr, "file:%s func:%s line:%d error, null length cvector!!\n", __FILE__, __func__, __LINE__);

        for (iter = cvector_begin(cv);
            iter != cvector_end(cv);
            iter = cvector_next(cv, iter))
        {
            cvector_iter_val(cv, iter, &num);
            printf("var:%d at:%d\n", num, (int)cvector_iter_at(cv, iter));
        }
    }
}

