struct weak_link_master;
struct weak_link_client;

struct weak_link_master * weak_link_new(void * p);
void weak_link_break(struct weak_link_master ** ppLink);

struct weak_link_client* weak_link_ref(struct weak_link_master * w);
void weak_link_unref(struct weak_link_client ** ppLink);

// Returns NULL if link broken - in this case it will also zap
//   *ppLink and unref the weak_link.
// Returns NULL if *ppLink is NULL (so a link once broken stays broken)
//
// The above does mean that there is a race if this is called simultainiously
// by two threads using the same weak_link_client (so don't do that)
void * weak_link_lock(struct weak_link_client ** ppLink);
void weak_link_unlock(struct weak_link_client * c);






