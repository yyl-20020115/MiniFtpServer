#include "trans_data.h"
#include "common.h"
#include "sysutil.h"
#include "ftp_codes.h"
#include "command_map.h"
#include "configure.h"
#include "priv_sock.h"
#include "trans_ctrl.h"

static char receiver_buffer[65535] = { 0 };

ssize_t send_file_block(SOCKET out_fd, int in_fd, long long* offset, size_t block_size) {
    int r = 0, d = 0;
    ssize_t n = 0;
    char buffer[4096] = { 0 };
    if (offset != 0) {
        _lseeki64(in_fd, *offset, SEEK_SET);
    }
    for (size_t i = 0; i < block_size; i+=sizeof(buffer)) {
        r = _read(in_fd, buffer, sizeof(buffer));
        d = send(out_fd, buffer, r, 0);
        n += d;
        if (r < sizeof(buffer)) {
            break;
        }
    }
    return n;
}

static int statbuf_get_perms(const char* buf, int szbuf,struct stat *sbuf);
static int statbuf_get_date(const char* buf, int szbuf, struct stat *sbuf);
static int statbuf_get_filename(const char* buf, int szbuf, struct stat *sbuf, const char *name);
static int statbuf_get_user_info(const char* buf, int szbuf, struct stat *sbuf);
static int statbuf_get_size(const char* buf, int szbuf, struct stat *sbuf);

static int is_port_active(Session_t *session);
static int is_pasv_active(Session_t *session);

static void get_port_data_fd(Session_t *session);
static void get_pasv_data_fd(Session_t *session);

static void trans_list_common(Session_t *session, int list);
static int get_trans_data_fd(Session_t *session);

#ifndef _WIN32
void limit_curr_rate(Session_t* sess, int nbytes, int is_upload)
{
    int curr_time_sec = get_curr_time_sec();
    int curr_time_usec = get_curr_time_usec();

    double elapsed = 0.0;
    elapsed += (curr_time_sec - sess->start_time_sec);
    elapsed += (curr_time_usec - sess->start_time_usec) / (double)1000000;
    if (elapsed < 0.000001)
        elapsed = 0.001;

    double curr_rate = nbytes / elapsed;

    double rate_radio = 0.0;
    if (is_upload)
    {
        if (sess->limits_max_upload > 0 && curr_rate > sess->limits_max_upload)
        {
            rate_radio = curr_rate / (sess->limits_max_upload);
        }
        else
        {
            sess->start_time_sec = get_curr_time_sec();
            sess->start_time_usec = get_curr_time_usec();
            return;
        }
    }
    else
    {
        if (sess->limits_max_download > 0 && curr_rate > sess->limits_max_download)
        {
            rate_radio = curr_rate / (sess->limits_max_download);
        }
        else
        {
            sess->start_time_sec = get_curr_time_sec();
            sess->start_time_usec = get_curr_time_usec();
            return;
        }
    }

    double sleep_time = (rate_radio - 1) * elapsed;

    if (n_sleep(sleep_time) == -1) {
        exit_with_error("nano_sleep");
        return;
    }
    sess->start_time_sec = get_curr_time_sec();
    sess->start_time_usec = get_curr_time_usec();
}
#endif

typedef ssize_t (*send_data_function)(SOCKET out_socket,void* src, long long* offset, size_t count);

int provide_data_as_file(Session_t* session, unsigned long long filesize, send_data_function sdf, void* src) {
    
    if (sdf == 0) return EXIT_FAILURE;

    session->is_translating_data = 1;
    {
        if (get_trans_data_fd(session) == 0)
        {
            ftp_reply(session, FTP_FILEFAIL, "Failed to open file.");
            return EXIT_SUCCESS;
        }

        long long offset = session->restart_pos;
        
        char text[1024] = { 0 };
        snprintf(text, sizeof(text),
            "Opening Binary mode data connection for %s (%llu bytes).",
            session->args, filesize);

        ftp_reply(session, FTP_DATACONN, text);

        int flag = 0;
        long long nleft = filesize;
        long long block_size = 0;
        const int kSize = 65536;
        while (nleft > 0)
        {
            block_size = (nleft > kSize) ? kSize : nleft;

            int nwrite = sdf(session->data_fd, src, &offset, block_size);

            if (session->is_receive_abor == 1)
            {
                flag = 2; //ABOR
                //426
                ftp_reply(session, FTP_BADSENDNET, "Interupt downloading file.");
                session->is_receive_abor = 0;
                break;
            }
            
            if (nwrite == -1)
            {
                flag = 1;
                break;
            }
            offset += nwrite;
            nleft -= nwrite;
        }
        if (nleft == 0)
            flag = 0;

        s_close(&session->data_fd);

        //226
        if (flag == 0)
            ftp_reply(session, FTP_TRANSFEROK, "Transfer complete.");
        else if (flag == 1)
            ftp_reply(session, FTP_BADSENDFILE, "Sendfile failed.");
        else if (flag == 2)
            ftp_reply(session, FTP_ABOROK, "ABOR successful.");
    }
    session->is_translating_data = 0;
    return EXIT_SUCCESS;
}
int download_file(Session_t *session)
{
    session->is_translating_data = 1;

    if(get_trans_data_fd(session) == 0)
    {
        ftp_reply(session, FTP_FILEFAIL, "Failed to open file.");
        return EXIT_SUCCESS;
    }

#ifndef _WIN32
    int fd = _open(session->args, O_RDONLY);
#else
    int fd = _open(session->args, O_RDONLY| O_BINARY);
#endif
    if(fd == -1)
    {
        ftp_reply(session, FTP_FILEFAIL, "Failed to open file.");
        return EXIT_SUCCESS;
    }
#ifndef _WIN32
    if(lock_file_read(fd) == -1)
    {
        ftp_reply(session, FTP_FILEFAIL, "Failed to open file.");
        return EXIT_SUCCESS;
    }
#endif
    struct stat sbuf;
    if (fstat(fd, &sbuf) == -1) {
        exit_with_error("fstat");
        return EXIT_SUCCESS;
    }
    if ((sbuf.st_mode & _S_IFREG) == 0)
    {
        ftp_reply(session, FTP_FILEFAIL, "Can only download regular file.");
        return EXIT_SUCCESS;
    }

    unsigned long long filesize = sbuf.st_size;
    long long offset = session->restart_pos;
    if(offset != 0)
    {
        filesize -= offset;
    }

    if (_lseeki64(fd, offset, SEEK_SET) == -1LL) {
        exit_with_error("lseek");
        return EXIT_SUCCESS;
    }
    //150 ascii
    //150 Opening ASCII mode data connection for /home/wing/redis-stable.tar.gz (1251318 bytes).
    char text[1024] = {0};
    if (session->ascii_mode == 1)
    {
        snprintf(text, sizeof(text), "Opening ASCII mode data connection for %s (%llu bytes).", session->args, filesize);
    }
    else 
    {
        snprintf(text, sizeof(text), "Opening Binary mode data connection for %s (%llu bytes).", session->args, filesize);
    }
    ftp_reply(session, FTP_DATACONN, text);

    session->start_time_sec = get_curr_time_sec();
    session->start_time_usec = get_curr_time_usec();

    int flag = 0;
    long long nleft = filesize;
    long long block_size = 0;
    const int kSize = 65536;
    while(nleft > 0)
    {
        block_size = (nleft > kSize) ? kSize : nleft;

        int nwrite = send_file_block(session->data_fd, fd, NULL, block_size);

        if(session->is_receive_abor == 1)
        {
            flag = 2; //ABOR
            //426
            ftp_reply(session, FTP_BADSENDNET, "Interupt downloading file.");
            session->is_receive_abor = 0;
            break;
        }

        if(nwrite == -1)
        {
            flag = 1;
            break;
        }
        nleft -= nwrite;
#ifndef _WIN32
        limit_curr_rate(session, nwrite, 0);
#endif
    }
    if(nleft == 0)
        flag = 0;
#ifndef _WIN32
    if (unlock_file(fd) == -1) {
        exit_with_error("unlock_file");
    }
#else

#endif
    _close(fd);
    s_close(&session->data_fd);
    //226
    if(flag == 0)
        ftp_reply(session, FTP_TRANSFEROK, "Transfer complete.");
    else if(flag == 1)
        ftp_reply(session, FTP_BADSENDFILE, "Sendfile failed.");
    else if(flag == 2)
        ftp_reply(session, FTP_ABOROK, "ABOR successful.");
#ifndef _WIN32
    setup_signal_alarm_ctrl_fd();
#endif
    session->is_translating_data = 0;    
    return EXIT_SUCCESS;
}


int upload_file(Session_t *session, int appending)
{
    session->is_translating_data = 1;
    if(get_trans_data_fd(session) == 0)
    {
        ftp_reply(session, FTP_UPLOADFAIL, "Failed to get data fd.");
        return EXIT_SUCCESS;
    }
#ifndef _WIN32
    int fd = _open(session->args, O_WRONLY | O_CREAT, 0666);
#else
    int fd = _open(session->args, O_WRONLY | O_CREAT | O_BINARY, 0666);
#endif
    if(fd == -1)
    {
        ftp_reply(session, FTP_UPLOADFAIL, "Failed to open file.");
        return EXIT_SUCCESS;
    }
#ifndef _WIN32
    if(lock_file_write(fd) == -1)
    {
        ftp_reply(session, FTP_UPLOADFAIL, "Failed to lock file.");
        return EXIT_SUCCESS;
    }
#endif
    struct stat sbuf;
    if (fstat(fd, &sbuf) == -1) {
        exit_with_error("fstat");
        return EXIT_SUCCESS;
    }
    if ((sbuf.st_mode & _S_IFREG) == 0)
    {
        ftp_reply(session, FTP_UPLOADFAIL, "Can only upload regular file.");
        return EXIT_SUCCESS;
    }
    long long offset = session->restart_pos;
    if(!appending && offset == 0) //STOR
    {
#ifndef _WIN32
        ftruncate(fd, 0);
#endif
    }
    else if(!appending && offset != 0) // REST + STOR
    {
#ifndef _WIN32
        ftruncate(fd, offset);
#endif
        if (_lseeki64(fd, offset, SEEK_SET) == -1LL) {
            exit_with_error("lseek");
            return EXIT_SUCCESS;
        }
    }
    else //APPE
    {
        if (_lseeki64(fd, 0, SEEK_END) == -1LL) {
            exit_with_error("lseek");
            return EXIT_SUCCESS;
        }
    }

    //150 ascii
    ftp_reply(session, FTP_DATACONN, "OK to send.");

    session->start_time_sec = get_curr_time_sec();
    session->start_time_usec = get_curr_time_usec();

    int flag = 0;
    int e = 0;
    for(;;)
    {
#ifndef _WIN32
        int nread = _read(session->data_fd, buf, sizeof(buf));
#else
        int nread = recv(session->data_fd, receiver_buffer, sizeof(receiver_buffer),0);
#endif
        if(session->is_receive_abor == 1)
        {
            flag = 3; //ABOR
            //426
            ftp_reply(session, FTP_BADSENDNET, "Interupt uploading file.");
            session->is_receive_abor = 0;
            break;
        }

        if(nread == -1)
        {
            if (
#ifndef _WIN32
                errno == EINTR
#else
                (e = WSAGetLastError()) == WSAEINTR
#endif
                )
                continue;
            flag = 1;
            break;
        }
        else if(nread == 0)
        {
            flag = 0;
            break;
        }

        if(writen(fd, receiver_buffer, nread) != nread)
        {
            flag = 2;
            break;
        }
#ifndef _WIN32
        limit_curr_rate(session, nread, 1);
#endif
    }
#ifndef _WIN32
    if (unlock_file(fd) == -1) {
        exit_with_error("unlock_file");
        return EXIT_SUCCESS;
    }
#endif
    _close(fd);
    s_close(&session->data_fd);

    //226
    if(flag == 0)
        ftp_reply(session, FTP_TRANSFEROK, "Transfer complete.");
    else if(flag == 1)
        ftp_reply(session, FTP_BADSENDNET, "Reading from Network Failed.");
    else if(flag == 2)
        ftp_reply(session, FTP_BADSENDFILE, "Writing to File Failed.");
    else
        ftp_reply(session, FTP_ABOROK, "ABOR successful.");
#ifndef _WIN32
    setup_signal_alarm_ctrl_fd();
#endif
    session->is_translating_data = 0;
    return EXIT_SUCCESS;
}

int trans_list(Session_t *session, int list)
{
    if(get_trans_data_fd(session) == 0)
        return EXIT_SUCCESS;

    //给出150 Here comes the directory listing.
    ftp_reply(session, FTP_DATACONN, "Here comes the directory listing.");

    if(list == 1)
        trans_list_common(session, 1);
    else
        trans_list_common(session, 0);

    s_close(&session->data_fd);
    
    ftp_reply(session, FTP_TRANSFEROK, "Directory send OK.");
    return EXIT_SUCCESS;
}

static int get_trans_data_fd(Session_t *session)
{
    int is_port = is_port_active(session);
    int is_pasv = is_pasv_active(session);

    if(!is_port && !is_pasv)
    {
        ftp_reply(session, FTP_BADSENDCONN, "Use PORT or PASV first.");
        return EXIT_SUCCESS;
    }
#if 0
    if (is_port && is_pasv)
    {
        exit_with_error("both of PORT and PASV are active\n");
        return EXIT_SUCCESS;
    }
#endif
    //port over pasv
    if(is_port)
    {
        get_port_data_fd(session);
    }
    else if(is_pasv)
    {
        get_pasv_data_fd(session);    
    }
    return 1;
}

static int statbuf_get_perms(const char* buf, int szbuf, struct stat *sbuf)
{
    char perms[] = "---------- ";
#ifndef _WIN32
    mode_t mode = sbuf->st_mode;

    switch(mode & S_IFMT)
    {
        case S_IFSOCK:
            perms[0] = 's';
            break;
        case S_IFLNK:
            perms[0] = 'l';
            break;
        case S_IFREG:
            perms[0] = '-';
            break;
        case S_IFBLK:
            perms[0] = 'b';
            break;
        case S_IFDIR:
            perms[0] = 'd';
            break;
        case S_IFCHR:
            perms[0] = 'c';
            break;
        case S_IFIFO:
            perms[0] = 'p';
            break;
    }

    if(mode & S_IRUSR)
        perms[1] = 'r';
    if(mode & S_IWUSR)
        perms[2] = 'w';
    if(mode & S_IXUSR)
        perms[3] = 'x';
    if(mode & S_IRGRP)
        perms[4] = 'r';
    if(mode & S_IWGRP)
        perms[5] = 'w';
    if(mode & S_IXGRP)
        perms[6] = 'x';
    if(mode & S_IROTH)
        perms[7] = 'r';
    if(mode & S_IWOTH)
        perms[8] = 'w';
    if(mode & S_IXOTH)
        perms[9] = 'x';

    if(mode & S_ISUID)
        perms[3] = (perms[3] == 'x') ? 's' : 'S';
    if(mode & S_ISGID)
        perms[6] = (perms[6] == 'x') ? 's' : 'S';
    if(mode & S_ISVTX)
        perms[9] = (perms[9] == 'x') ? 't' : 'T';
#endif
    int sl = (int)strlen(buf);
    int dt = szbuf - sl;
    
    return (dt > strlen(perms)) ? snprintf((char* const)(buf + sl), dt, perms) : 0;
}

static int statbuf_get_date(const char* buf, int szbuf, struct stat *sbuf)
{
    struct tm *ptm = 0;
    time_t ct = sbuf->st_ctime;
    if ((ptm = localtime(&ct)) == NULL) {
        exit_with_error("localtime");
        return -1;
    }
    int r = 0;
    const char *format = " %b %e %hh:%mm";
    char buffer[1024] = { 0 };
    if(r = (strftime(buffer, sizeof(buffer), format, ptm)) == 0)
    {
        exit_with_error("strftime error\n");
    }
    int bl = (int)strlen(buf);

    snprintf((char* const)(buf + bl), (size_t)szbuf - bl, "%s", buffer);

    return r;
}

static int statbuf_get_filename(const char* buf, int szbuf, struct stat *sbuf, const char *name)
{
    char filename[1024] = {0};
#ifndef _WIN32
    if(S_ISLNK(sbuf->st_mode))
    {
        char linkfile[1024] = {0};
        if (readlink(name, linkfile, sizeof linkfile) == -1) {
            exit_with_error("readlink");
        }
        return snprintf((char* const)(buf + strlen(buf)), szbuf, "%s -> %s", name, linkfile);
    }
    else
#endif
    {
        int sl = (int)strlen(buf);
        int dt = szbuf - sl;

        return snprintf((char* const)(buf + sl), dt, name);
    }
}

static int statbuf_get_user_info(const char* buf, int szbuf, struct stat *sbuf)
{
    int sl = (int)strlen(buf);
    int dt = szbuf - sl;
    return dt<=23 ? 0: snprintf((char* const)(buf + sl), dt, " %3d %8d %8d ", sbuf->st_nlink, sbuf->st_uid, sbuf->st_gid);
}

static int statbuf_get_size(const char* buf, int szbuf, struct stat *sbuf)
{
    int sl = (int)strlen(buf);
    int dt = szbuf - sl;
    return dt<=9 ? 0 : snprintf((char* const)(buf+sl), dt, "%8lu ", (unsigned long)sbuf->st_size);
}

static int is_port_active(Session_t *session)
{
    return (session->p_addr != NULL);
}

static int is_pasv_active(Session_t *session)
{
    priv_sock_send_cmd(session->proto_fd, PRIV_SOCK_PASV_ACTIVE);
    int res = 0;
    priv_sock_recv_int(session->proto_fd,&res);
    return res;
}

static void get_port_data_fd(Session_t *session)
{
    priv_sock_send_cmd(session->proto_fd, PRIV_SOCK_GET_DATA_SOCK);

    char *ip = inet_ntoa(session->p_addr->sin_addr);
    uint16_t port = ntohs(session->p_addr->sin_port);
    priv_sock_send_str(session->proto_fd, ip, (unsigned int)strlen(ip));
    priv_sock_send_int(session->proto_fd, port);

    char result = 0;
    priv_sock_recv_result(session->proto_fd,&result);
    if(result == PRIV_SOCK_RESULT_BAD)
    {
        ftp_reply(session, FTP_BADCMD, "get pasv data_fd error");
        exit_with_error("get data fd error\n");
    }

    priv_sock_recv_fd(session->proto_fd,&session->data_fd);

    free(session->p_addr);
    session->p_addr = NULL;
}

static void get_pasv_data_fd(Session_t *session)
{
    priv_sock_send_cmd(session->proto_fd, PRIV_SOCK_PASV_ACCEPT);

    char res = 0;
    
    priv_sock_recv_result(session->proto_fd,&res);

    if(res == PRIV_SOCK_RESULT_BAD)
    {
        ftp_reply(session, FTP_BADCMD, "get pasv data_fd error");
        exit_with_error("get data fd error\n");
    }

    priv_sock_recv_fd(session->proto_fd,&session->data_fd);
}

#ifdef _WIN32
typedef struct _dirdesc {
    int     dd_fd;      /** file descriptor associated with directory */
    long    dd_loc;     /** offset in current buffer */
    long    dd_size;    /** amount of data returned by getdirentries */
    char* dd_buf;    /** data buffer */
    int     dd_len;     /** size of data buffer */
    long    dd_seek;    /** magic cookie returned by getdirentries */
    HANDLE hFind;
} DIR;
struct dirent
{
    long d_ino;              /* inode number*/
    off_t d_off;             /* offset to this dirent*/
    unsigned short d_reclen; /* length of this d_name*/
    unsigned char d_type;    /* the type of d_name*/
    char d_name[1];          /* file name (null-terminated)*/
};
DIR* opendir(const char* name)
{
    DIR* dir = 0;
    WIN32_FIND_DATAA FindData = { 0 };
    char namebuf[512] = { 0 };

    sprintf(namebuf, "%s\\*.*", name);

    HANDLE hFind = FindFirstFileA(namebuf, &FindData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        exit_with_error("FindFirstFile failed (%d)\n", GetLastError());
        return 0;
    }

    dir = (DIR*)malloc(sizeof(DIR));
    if (!dir)
    {
        exit_with_error("DIR memory allocate fail\n");
        return 0;
    }

    memset(dir, 0, sizeof(DIR));
    dir->dd_fd = 0;   // simulate return  
    dir->hFind = hFind;
    return dir;
}

struct dirent* readdir(DIR* d)
{
    if (d == 0) return 0;

    int i = 0;
    WIN32_FIND_DATAA FileData = { 0 }; 
    BOOL bf = FindNextFileA(d->hFind, &FileData);
    //fail or end  
    if (!bf) return 0;

    struct dirent* dirent = (struct dirent*)malloc(
        sizeof(struct dirent) + sizeof(FileData.cFileName));
    if (dirent != 0) 
    {
        for (i = 0; i < sizeof(FileData.cFileName); i++)
        {
            dirent->d_name[i] = FileData.cFileName[i];
            if (FileData.cFileName[i] == '\0') break;
        }
        dirent->d_reclen = i;
   
        //check there is file or directory  
        if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            dirent->d_type = 2;
        }
        else
        {
            dirent->d_type = 1;
        }
    }
    return dirent;
} 
int closedir(DIR* d)
{
    if (d == 0) return -1;
    if (d->hFind != INVALID_HANDLE_VALUE) {
        FindClose(d->hFind);
    }
    free(d);
    return 0;
}
#endif
static void trans_list_common(Session_t *session, int list)
{
    DIR *dir = opendir(".");
    if (dir == NULL) {
        exit_with_error("opendir");
        return;
    }
    struct dirent *dr = 0;
    while((dr = readdir(dir))!=0)
    {
        const char *filename = dr->d_name;
        if (filename[0] == '.') {
#ifdef _WIN32
            free(dr);
#endif
            continue;
        }
        char buf[4096] = {0};
        int szbuf = sizeof(buf);
        struct stat sbuf = { 0 };
        if (stat(filename, &sbuf) == -1)
        {
#ifdef _WIN32
            free(dr);
#endif
            exit_with_error("lstat");
            return;
        }
        if(list == 1) // LIST
        {
            statbuf_get_perms(buf, szbuf, &sbuf);            
            statbuf_get_user_info(buf,szbuf, &sbuf);
            statbuf_get_size(buf, szbuf, &sbuf);
            statbuf_get_date(buf, szbuf, &sbuf);
            statbuf_get_filename(buf, szbuf, &sbuf, filename);
        }
        else //NLST
        {
            statbuf_get_filename(buf, szbuf ,&sbuf, filename);
        }
        int slb = (int)strlen(buf);
        if (slb < szbuf-2) {
            strcat(buf, "\r\n");
        }
        slb = (int)strlen(buf);
        if (slb < szbuf) {
            buf[slb] = '\0';
        }
        else {
            buf[szbuf - 1] = '\0';
        }
        writes(session->data_fd, buf);
#ifdef _WIN32
        free(dr);
#endif
    }

    closedir(dir);
}

