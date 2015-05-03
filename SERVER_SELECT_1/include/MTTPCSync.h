#ifndef MTTPCSYNC_H
#define MTTPCSYNC_H


class MTTPCSync
{
public:
    bool Init();
    bool Start();

    protected:
private:
    int m_l_sock;
};

#endif // MTTPCSYNC_H
