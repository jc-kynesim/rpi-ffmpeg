struct ff_weak_link_master;
struct ff_weak_link_client;

struct ff_weak_link_master * ff_weak_link_new(void * p);
void ff_weak_link_break(struct ff_weak_link_master ** ppLink);

struct ff_weak_link_client* ff_weak_link_ref(struct ff_weak_link_master * w);
void ff_weak_link_unref(struct ff_weak_link_client ** ppLink);

// Returns NULL if link broken - in this case it will also zap
//   *ppLink and unref the weak_link.
// Returns NULL if *ppLink is NULL (so a link once broken stays broken)
//
// The above does mean that there is a race if this is called simultainiously
// by two threads using the same weak_link_client (so don't do that)
void * ff_weak_link_lock(struct ff_weak_link_client ** ppLink);
void ff_weak_link_unlock(struct ff_weak_link_client * c);






