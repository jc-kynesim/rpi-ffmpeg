#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/sysmacros.h>

#include <linux/media.h>
#include <linux/videodev2.h>

#include "v4l2_req_devscan.h"
#include "v4l2_req_utils.h"

struct decdev {
    enum v4l2_buf_type src_type;
    uint32_t src_fmt_v4l2;
    const char * vname;
    const char * mname;
};

struct devscan {
    struct decdev env;
    unsigned int dev_size;
    unsigned int dev_count;
    struct decdev *devs;
};

static int video_src_pixfmt_supported(uint32_t fmt)
{
    return 1;
}

static void v4l2_setup_format(struct v4l2_format *format, unsigned int type,
                  unsigned int width, unsigned int height,
                  unsigned int pixelformat)
{
    unsigned int sizeimage;

    memset(format, 0, sizeof(*format));
    format->type = type;

    sizeimage = V4L2_TYPE_IS_OUTPUT(type) ? 4 * 1024 * 1024 : 0;

    if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
        format->fmt.pix_mp.width = width;
        format->fmt.pix_mp.height = height;
        format->fmt.pix_mp.plane_fmt[0].sizeimage = sizeimage;
        format->fmt.pix_mp.pixelformat = pixelformat;
    } else {
        format->fmt.pix.width = width;
        format->fmt.pix.height = height;
        format->fmt.pix.sizeimage = sizeimage;
        format->fmt.pix.pixelformat = pixelformat;
    }
}

static int v4l2_set_format(int video_fd, unsigned int type, unsigned int pixelformat,
            unsigned int width, unsigned int height)
{
    struct v4l2_format format;

    v4l2_setup_format(&format, type, width, height, pixelformat);

    return ioctl(video_fd, VIDIOC_S_FMT, &format) ? -errno : 0;
}

static int v4l2_query_capabilities(int video_fd, unsigned int *capabilities)
{
    struct v4l2_capability capability = { 0 };
    int rc;

    rc = ioctl(video_fd, VIDIOC_QUERYCAP, &capability);
    if (rc < 0)
        return -errno;

    if (capabilities != NULL) {
        if ((capability.capabilities & V4L2_CAP_DEVICE_CAPS) != 0)
            *capabilities = capability.device_caps;
        else
            *capabilities = capability.capabilities;
    }

    return 0;
}

static int devscan_add(struct devscan *const scan,
                       enum v4l2_buf_type src_type,
                       uint32_t src_fmt_v4l2,
                       const char * vname,
                       const char * mname)
{
    struct decdev *d;

    if (scan->dev_size <= scan->dev_count) {
        unsigned int n = !scan->dev_size ? 4 : scan->dev_size * 2;
        d = realloc(scan->devs, n * sizeof(*d));
        if (!d)
            return -ENOMEM;
        scan->devs = d;
        scan->dev_size = n;
    }

    d = scan->devs + scan->dev_count;
    d->src_type = src_type;
    d->src_fmt_v4l2 = src_fmt_v4l2;
    d->vname = strdup(vname);
    if (!d->vname)
        return -ENOMEM;
    d->mname = strdup(mname);
    if (!d->mname) {
        free((char *)d->vname);
        return -ENOMEM;
    }
    ++scan->dev_count;
    return 0;
}

void devscan_delete(struct devscan **const pScan)
{
    unsigned int i;
    struct devscan * const scan = *pScan;

    if (!scan)
        return;
    *pScan = NULL;

    for (i = 0; i < scan->dev_count; ++i) {
        free((char*)scan->devs[i].mname);
        free((char*)scan->devs[i].vname);
    }
    free(scan->devs);
    free(scan);
}

#define REQ_BUF_CAPS (\
    V4L2_BUF_CAP_SUPPORTS_DMABUF |\
    V4L2_BUF_CAP_SUPPORTS_REQUESTS |\
    V4L2_BUF_CAP_SUPPORTS_M2M_HOLD_CAPTURE_BUF)

static void probe_formats(void * const dc,
              struct devscan *const scan,
              const int fd,
              const unsigned int type_v4l2,
              const char *const mpath,
              const char *const vpath)
{
    unsigned int i;
    for (i = 0;; ++i) {
        struct v4l2_fmtdesc fmtdesc = {
            .index = i,
            .type = type_v4l2
        };
        struct v4l2_requestbuffers rbufs = {
            .count = 0,
            .type = type_v4l2,
            .memory = V4L2_MEMORY_MMAP
        };
        while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
            if (errno == EINTR)
                continue;
            if (errno != EINVAL)
                request_err(dc, "Enum[%d] failed for type=%d\n", i, type_v4l2);
            return;
        }
        if (!video_src_pixfmt_supported(fmtdesc.pixelformat))
            continue;

        if (v4l2_set_format(fd, type_v4l2, fmtdesc.pixelformat, 720, 480)) {
            request_debug(dc, "Set failed for type=%d, pf=%.4s\n", type_v4l2, (char*)&fmtdesc.pixelformat);
            continue;
        }

        while (ioctl(fd, VIDIOC_REQBUFS, &rbufs)) {
            if (errno != EINTR) {
                request_debug(dc, "%s: Reqbufs failed\n", vpath);
                continue;
            }
        }

        if ((rbufs.capabilities & REQ_BUF_CAPS) != REQ_BUF_CAPS) {
            request_debug(dc, "%s: Buf caps %#x insufficient\n", vpath, rbufs.capabilities);
            continue;
        }

        request_debug(dc, "Adding: %s,%s pix=%#x, type=%d\n",
                 mpath, vpath, fmtdesc.pixelformat, type_v4l2);
        devscan_add(scan, type_v4l2, fmtdesc.pixelformat, vpath, mpath);
    }
}


static int probe_video_device(void * const dc,
                   struct udev_device *const device,
                   struct devscan *const scan,
                   const char *const mpath)
{
    int ret;
    unsigned int capabilities = 0;
    int video_fd = -1;

    const char *path = udev_device_get_devnode(device);
    if (!path) {
        request_err(dc, "%s: get video device devnode failed\n", __func__);
        ret = -EINVAL;
        goto fail;
    }

    video_fd = open(path, O_RDWR, 0);
    if (video_fd == -1) {
        ret = -errno;
        request_err(dc, "%s: opening %s failed, %s (%d)\n", __func__, path, strerror(errno), errno);
        goto fail;
    }

    ret = v4l2_query_capabilities(video_fd, &capabilities);
    if (ret < 0) {
        request_err(dc, "%s: get video capability failed, %s (%d)\n", __func__, strerror(-ret), -ret);
        goto fail;
    }

    request_debug(dc, "%s: path=%s capabilities=%#x\n", __func__, path, capabilities);

    if (!(capabilities & V4L2_CAP_STREAMING)) {
        request_debug(dc, "%s: missing required streaming capability\n", __func__);
        ret = -EINVAL;
        goto fail;
    }

    if (!(capabilities & (V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_VIDEO_M2M))) {
        request_debug(dc, "%s: missing required mem2mem capability\n", __func__);
        ret = -EINVAL;
        goto fail;
    }

    /* Should check capture formats too... */
    if ((capabilities & V4L2_CAP_VIDEO_M2M) != 0)
        probe_formats(dc, scan, video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, mpath, path);
    if ((capabilities & V4L2_CAP_VIDEO_M2M_MPLANE) != 0)
        probe_formats(dc, scan, video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, mpath, path);

    close(video_fd);
    return 0;

fail:
    if (video_fd >= 0)
        close(video_fd);
    return ret;
}

static const char * entity_type_to_str(const unsigned int etype, char tbuf[32])
{
    switch (etype) {
        case MEDIA_ENT_F_UNKNOWN:
            return "UNKNOWN";
        case MEDIA_ENT_F_V4L2_SUBDEV_UNKNOWN:
            return "V4L2_SUBDEV_UNKNOWN";
        case MEDIA_ENT_F_IO_V4L:
            return "IO_V4L";
        case MEDIA_ENT_F_IO_VBI:
            return "IO_VBI";
        case MEDIA_ENT_F_IO_SWRADIO:
            return "IO_SWRADIO";
        case MEDIA_ENT_F_IO_DTV:
            return "IO_DTV";
        case MEDIA_ENT_F_DTV_DEMOD:
            return "DTV_DEMOD";
        case MEDIA_ENT_F_TS_DEMUX:
            return "TS_DEMUX";
        case MEDIA_ENT_F_DTV_CA:
            return "DTV_CA";
        case MEDIA_ENT_F_DTV_NET_DECAP:
            return "DTV_NET_DECAP";
#ifdef MEDIA_ENT_F_CONN_RF
        case MEDIA_ENT_F_CONN_RF:
            return "CONN_RF";
#endif
#ifdef MEDIA_ENT_F_CONN_SVIDEO
        case MEDIA_ENT_F_CONN_SVIDEO:
            return "CONN_SVIDEO";
#endif
#ifdef MEDIA_ENT_F_CONN_COMPOSITE
        case MEDIA_ENT_F_CONN_COMPOSITE:
            return "CONN_COMPOSITE";
#endif
        case MEDIA_ENT_F_CAM_SENSOR:
            return "CAM_SENSOR";
        case MEDIA_ENT_F_FLASH:
            return "FLASH";
        case MEDIA_ENT_F_LENS:
            return "LENS";
        case MEDIA_ENT_F_ATV_DECODER:
            return "ATV_DECODER";
        case MEDIA_ENT_F_TUNER:
            return "TUNER";
        case MEDIA_ENT_F_IF_VID_DECODER:
            return "IF_VID_DECODER";
        case MEDIA_ENT_F_IF_AUD_DECODER:
            return "IF_AUD_DECODER";
        case MEDIA_ENT_F_AUDIO_CAPTURE:
            return "AUDIO_CAPTURE";
        case MEDIA_ENT_F_AUDIO_PLAYBACK:
            return "AUDIO_PLAYBACK";
        case MEDIA_ENT_F_AUDIO_MIXER:
            return "AUDIO_MIXER";
        case MEDIA_ENT_F_PROC_VIDEO_COMPOSER:
            return "PROC_VIDEO_COMPOSER";
        case MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER:
            return "PROC_VIDEO_PIXEL_FORMATTER";
        case MEDIA_ENT_F_PROC_VIDEO_PIXEL_ENC_CONV:
            return "PROC_VIDEO_PIXEL_ENC_CONV";
        case MEDIA_ENT_F_PROC_VIDEO_LUT:
            return "PROC_VIDEO_LUT";
        case MEDIA_ENT_F_PROC_VIDEO_SCALER:
            return "PROC_VIDEO_SCALER";
        case MEDIA_ENT_F_PROC_VIDEO_STATISTICS:
            return "PROC_VIDEO_STATISTICS";
        case MEDIA_ENT_F_PROC_VIDEO_ENCODER:
            return "PROC_VIDEO_ENCODER";
        case MEDIA_ENT_F_PROC_VIDEO_DECODER:
            return "PROC_VIDEO_DECODER";
        case MEDIA_ENT_F_PROC_VIDEO_ISP:
            return "PROC_VIDEO_ISP";
        case MEDIA_ENT_F_VID_MUX:
            return "VID_MUX";
        case MEDIA_ENT_F_VID_IF_BRIDGE:
            return "VID_IF_BRIDGE";
        case MEDIA_ENT_F_DV_DECODER:
            return "DV_DECODER";
        case MEDIA_ENT_F_DV_ENCODER:
            return "DV_ENCODER";
        default:
            break;
    }
    if (tbuf != NULL)
        snprintf(tbuf, 32, "type:%#x", etype);
    return tbuf;
}

static const char * link_flags_type_to_str(const unsigned int lflags, char tbuf[32])
{
    switch (lflags & MEDIA_LNK_FL_LINK_TYPE) {
        case MEDIA_LNK_FL_DATA_LINK:
            return "DATA_LINK";
        case MEDIA_LNK_FL_INTERFACE_LINK:
            return "INTERFACE_LINK";
        case MEDIA_LNK_FL_ANCILLARY_LINK:
            return "ANCILLARY_LINK";
        default:
            break;
    }
    if (tbuf != NULL)
        snprintf(tbuf, 32, "type:%#x", lflags & MEDIA_LNK_FL_LINK_TYPE);
    return tbuf;
}

static const char * pad_flags_type_to_str(const unsigned int pflags, char tbuf[32])
{
    switch (pflags & (MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_SOURCE)) {
        case MEDIA_PAD_FL_SINK:
            return "SINK";
        case MEDIA_PAD_FL_SOURCE:
            return "SOURCE";
        default:
            break;
    }
    if (tbuf != NULL)
        snprintf(tbuf, 32, "type:%#x", pflags & (MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_SOURCE));
    return tbuf;
}

#define LINK_IS_DATA(link) (((link)->flags & MEDIA_LNK_FL_LINK_TYPE) == MEDIA_LNK_FL_DATA_LINK)
#define LINK_IS_INTERFACE(link) (((link)->flags & MEDIA_LNK_FL_LINK_TYPE) == MEDIA_LNK_FL_INTERFACE_LINK)

// Simple linear search - we could sort the arrays & chop
static const struct media_v2_link *
link_find_data_source(const struct media_v2_link * link,
                      const unsigned int n, const uint32_t source_id)
{
    for (unsigned int i = 0; i != n; ++i, ++link) {
        if (link->source_id == source_id && LINK_IS_DATA(link))
            return link;
    }
    return NULL;
}
static const struct media_v2_link *
link_find_data_sink(const struct media_v2_link * link,
                    const unsigned int n, const uint32_t sink_id)
{
    for (unsigned int i = 0; i != n; ++i, ++link) {
        if (link->sink_id == sink_id && LINK_IS_DATA(link))
            return link;
    }
    return NULL;
}
static const struct media_v2_link *
link_find_interface_entity(const struct media_v2_link * link,
                    const unsigned int n, const uint32_t entity_id)
{
    for (unsigned int i = 0; i != n; ++i, ++link) {
        if (link->sink_id == entity_id && LINK_IS_INTERFACE(link))
            return link;
    }
    return NULL;
}

static const struct media_v2_pad *
pad_find_id(const struct media_v2_pad  * pad, const unsigned int n, const uint32_t pad_id)
{
    for (unsigned int i = 0; i != n; ++i, ++pad) {
        if (pad->id == pad_id)
            return pad;
    }
    return NULL;
}
static const struct media_v2_interface *
interface_find_id(const struct media_v2_interface * interface, const unsigned int n, const uint32_t if_id)
{
    for (unsigned int i = 0; i != n; ++i, ++interface) {
        if (interface->id == if_id)
            return interface;
    }
    return NULL;
}

static const struct media_v2_pad *
pad_find_linked_pad(const struct media_v2_link links[const], const unsigned int n_links,
                    const struct media_v2_pad pads[const], const unsigned int n_pads,
                    const struct media_v2_pad * const pad1)
{
    const struct media_v2_link * link = NULL;
    uint32_t id;

    switch (pad1->flags & (MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_SOURCE)) {
        case MEDIA_PAD_FL_SINK:
            link = link_find_data_sink(links, n_links, pad1->id);
            if (!link)
                return NULL;
            id = link->source_id;
            break;
        case MEDIA_PAD_FL_SOURCE:
            link = link_find_data_source(links, n_links, pad1->id);
            if (!link)
                return NULL;
            id = link->sink_id;
            break;
        default:
            return NULL;
    }
    if (!link)
        return NULL;

    return pad_find_id(pads, n_pads, id);
}

static int probe_media_device(void * const dc,
                   struct udev_device *const device,
                   struct devscan *const scan)
{
    int ret;
    int rv;
    struct media_device_info device_info = { 0 };
    struct media_v2_topology topology = { 0 };
    struct media_v2_interface *interfaces = NULL;
    struct media_v2_entity *entities = NULL;
    struct media_v2_link *links = NULL;
    struct media_v2_pad *pads = NULL;
    uint32_t topology_version = 0;
    struct udev *udev = udev_device_get_udev(device);
    struct udev_device *video_device;
    dev_t devnum;
    int media_fd = -1;

    const char *path = udev_device_get_devnode(device);
    if (!path) {
        request_err(dc, "%s: get media device devnode failed\n", __func__);
        ret = -EINVAL;
        goto fail;
    }

    media_fd = open(path, O_RDWR, 0);
    if (media_fd < 0) {
        ret = -errno;
        request_err(dc, "%s: opening %s failed, %s (%d)\n", __func__, path, strerror(-ret), -ret);
        goto fail;
    }

    rv = ioctl(media_fd, MEDIA_IOC_DEVICE_INFO, &device_info);
    if (rv < 0) {
        ret = -errno;
        request_err(dc, "%s: get media device info failed, %s (%d)\n", __func__, strerror(-ret), -ret);
        goto fail;
    }

    for (;;) {
        rv = ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology);
        if (rv < 0) {
            ret = -errno;
            request_err(dc, "%s: get media topology failed, %s (%d)\n", __func__, strerror(-ret), -ret);
            goto fail;
        }

        if (topology.num_interfaces <= 0) {
            request_err(dc, "%s: media device has no interfaces\n", __func__);
            ret = -EINVAL;
            goto fail;
        }

        if (interfaces && topology_version == topology.topology_version)
            break;

        free(interfaces);
        free(entities);
        free(links);
        free(pads);

        interfaces = calloc(topology.num_interfaces, sizeof(*interfaces));
        entities = calloc(topology.num_entities, sizeof(*entities));
        links = calloc(topology.num_links, sizeof(*links));
        pads = calloc(topology.num_links, sizeof(*pads));
        if (!interfaces || !entities || !links || !pads) {
            request_err(dc, "%s: allocating media interface structs failed\n", __func__);
            ret = -ENOMEM;
            goto fail;
        }

        topology.ptr_interfaces = (__u64)(uintptr_t)interfaces;
        topology.ptr_entities = (__u64)(uintptr_t)entities;
        topology.ptr_links = (__u64)(uintptr_t)links;
        topology.ptr_pads = (__u64)(uintptr_t)pads;
    }

    {
        for (uint32_t i = 0; i < topology.num_entities; ++i) {
            char tnamebuf[32];
            const struct media_v2_entity *e = entities + i;
            request_info(dc, "%s: Entity id: %d, name '%s', type %s, flags %#x\n", __func__,
                         e->id, e->name, entity_type_to_str(e->function, tnamebuf), e->flags);
        }
    }
    {
        for (uint32_t i = 0; i < topology.num_links; ++i) {
            char tnamebuf[32];
            const struct media_v2_link *e = links + i;
            request_info(dc, "%s: Link id: %d, %d -> %d, type %s, flags %#x\n", __func__,
                         e->id, e->source_id, e->sink_id, link_flags_type_to_str(e->flags, tnamebuf), e->flags);
        }
    }
    {
        for (uint32_t i = 0; i < topology.num_pads; ++i) {
            char tnamebuf[32];
            const struct media_v2_pad *e = pads + i;
            request_info(dc, "%s: Pad id: %d, ent %d, type %s, flags %#x\n", __func__,
                         e->id, e->entity_id, pad_flags_type_to_str(e->flags, tnamebuf), e->flags);
        }
    }

    for (uint32_t i = 0; i < topology.num_entities; ++i) {
        const struct media_v2_entity *e = entities + i;
        if (e->function != MEDIA_ENT_F_PROC_VIDEO_DECODER)
            continue;
        // Find pads attached to decoder
        for (uint32_t j = 0; j < topology.num_pads; ++j) {
            const struct media_v2_pad *p = pads + j;
            const struct media_v2_pad *p2;
            const struct media_v2_link *link2;
            const struct media_v2_interface *interface;
            if (p->entity_id != e->id)
                continue;
            // Find pad link
            // Find pad at the other end of the link
            p2 = pad_find_linked_pad(links, topology.num_links, pads, topology.num_pads, p);
            if (!p2)
                continue;
            // Find interface link to pad->entity
            link2 = link_find_interface_entity(links, topology.num_links, p2->entity_id);
            if (!link2)
                continue;
            // Find interface
            interface = interface_find_id(interfaces, topology.num_interfaces, link2->source_id);
            if (!interface)
                continue;
            request_info(dc, "Found interface! id=%d\n", interface->id);
        }

    }


    for (int i = 0; i < topology.num_interfaces; i++) {
        if (interfaces[i].intf_type != MEDIA_INTF_T_V4L_VIDEO)
            continue;

        devnum = makedev(interfaces[i].devnode.major, interfaces[i].devnode.minor);
        video_device = udev_device_new_from_devnum(udev, 'c', devnum);
        if (!video_device) {
            ret = -errno;
            request_err(dc, "%s: video_device[%d]=%p\n", __func__, i, video_device);
            continue;
        }

        ret = probe_video_device(dc, video_device, scan, path);
        udev_device_unref(video_device);

        if (ret != 0)
            goto fail;
    }

fail:
    free(interfaces);
    free(entities);
    free(links);
    free(pads);
    if (media_fd != -1)
        close(media_fd);
    return ret;
}

const char *decdev_media_path(const struct decdev *const dev)
{
    return !dev ? NULL : dev->mname;
}

const char *decdev_video_path(const struct decdev *const dev)
{
    return !dev ? NULL : dev->vname;
}

enum v4l2_buf_type decdev_src_type(const struct decdev *const dev)
{
    return !dev ? 0 : dev->src_type;
}

uint32_t decdev_src_pixelformat(const struct decdev *const dev)
{
    return !dev ? 0 : dev->src_fmt_v4l2;
}


const struct decdev *devscan_find(struct devscan *const scan,
                  const uint32_t src_fmt_v4l2)
{
    unsigned int i;

    if (scan->env.mname && scan->env.vname)
        return &scan->env;

    if (!src_fmt_v4l2)
        return scan->dev_count ? scan->devs + 0 : NULL;

    for (i = 0; i != scan->dev_count; ++i) {
        if (scan->devs[i].src_fmt_v4l2 == src_fmt_v4l2)
            return scan->devs + i;
    }
    return NULL;
}

int devscan_build(void * const dc, struct devscan **pscan)
{
    int ret;
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices;
    struct udev_list_entry *entry;
    struct udev_device *device;
    struct devscan * scan;

    *pscan = NULL;

    scan = calloc(1, sizeof(*scan));
    if (!scan) {
        ret = -ENOMEM;
        goto fail;
    }

    scan->env.mname = getenv("LIBVA_V4L2_REQUEST_MEDIA_PATH");
    scan->env.vname = getenv("LIBVA_V4L2_REQUEST_VIDEO_PATH");
    if (scan->env.mname && scan->env.vname) {
        request_info(dc, "Media/video device env overrides found: %s,%s\n",
                 scan->env.mname, scan->env.vname);
        *pscan = scan;
        return 0;
    }

    udev = udev_new();
    if (!udev) {
        request_err(dc, "%s: allocating udev context failed\n", __func__);
        ret = -ENOMEM;
        goto fail;
    }

    enumerate = udev_enumerate_new(udev);
    if (!enumerate) {
        request_err(dc, "%s: allocating udev enumerator failed\n", __func__);
        ret = -ENOMEM;
        goto fail;
    }

    udev_enumerate_add_match_subsystem(enumerate, "media");
    udev_enumerate_scan_devices(enumerate);

    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(entry, devices) {
        const char *path = udev_list_entry_get_name(entry);
        if (!path)
            continue;

        device = udev_device_new_from_syspath(udev, path);
        if (!device)
            continue;

        probe_media_device(dc, device, scan);
        udev_device_unref(device);
    }

    udev_enumerate_unref(enumerate);

    *pscan = scan;
    return 0;

fail:
    udev_unref(udev);
    devscan_delete(&scan);
    return ret;
}

