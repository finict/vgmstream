#include "layout.h"
#include "../vgmstream.h"
#include "../coding/coding.h"

segmented_layout_data* init_layout_segmented(int segment_count) {
    segmented_layout_data *data = NULL;

    if (segment_count <= 0 || segment_count > 255)
        goto fail;

    data = calloc(1, sizeof(segmented_layout_data));
    if (!data) goto fail;

    data->segment_count = segment_count;
    data->current_segment = 0;

    data->segments = calloc(segment_count, sizeof(VGMSTREAM*));
    if (!data->segments) goto fail;

    return data;
fail:
    free_layout_segmented(data);
    return NULL;
}


int setup_layout_segmented(segmented_layout_data* data) {
    int i;

    /* setup each VGMSTREAM (roughly equivalent to vgmstream.c's init_vgmstream_internal stuff) */
    for (i = 0; i < data->segment_count; i++) {
        if (!data->segments[i])
            goto fail;

        if (data->segments[i]->num_samples <= 0)
            goto fail;

        /* shouldn't happen */
        if (data->segments[i]->loop_flag != 0) {
            VGM_LOG("segmented layout: segment %i is looped\n", i);
            data->segments[i]->loop_flag = 0;
        }

        if (i > 0) {
            if (data->segments[i]->channels != data->segments[i-1]->channels)
                goto fail;

            /* a bit weird, but no matter */
            if (data->segments[i]->sample_rate != data->segments[i-1]->sample_rate) {
                VGM_LOG("segmented layout: segment %i has different sample rate\n", i);
            }

            //if (data->segments[i]->coding_type != data->segments[i-1]->coding_type)
            //    goto fail; /* perfectly acceptable */
        }


        /* save start things so we can restart for seeking/looping */
        memcpy(data->segments[i]->start_ch,data->segments[i]->ch,sizeof(VGMSTREAMCHANNEL)*data->segments[i]->channels);
        memcpy(data->segments[i]->start_vgmstream,data->segments[i],sizeof(VGMSTREAM));
    }


    return 1;
fail:
    return 0; /* caller is expected to free */
}


void render_vgmstream_segmented(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    int samples_written=0;
    segmented_layout_data *data = vgmstream->layout_data;
    //int samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);

    while (samples_written<sample_count) {
        int samples_to_do;
        int samples_this_block = data->segments[data->current_segment]->num_samples;

        if (vgmstream->loop_flag && vgmstream_do_loop(vgmstream)) {
            //todo can only loop in a segment start
            // (for arbitrary values find loop segment from loop_start_sample, and skip N samples until loop start)
            data->current_segment = data->loop_segment;

            reset_vgmstream(data->segments[data->current_segment]);

            vgmstream->samples_into_block = 0;
            continue;
        }

        samples_to_do = vgmstream_samples_to_do(samples_this_block, 1, vgmstream);

        if (samples_written+samples_to_do > sample_count)
            samples_to_do=sample_count-samples_written;

        if (samples_to_do == 0) {
            data->current_segment++;
            reset_vgmstream(data->segments[data->current_segment]);

            vgmstream->samples_into_block = 0;
            continue;
        }

        render_vgmstream(&buffer[samples_written*data->segments[data->current_segment]->channels],
                samples_to_do,data->segments[data->current_segment]);

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block+=samples_to_do;
    }
}


void free_layout_segmented(segmented_layout_data *data) {
    int i;

    if (!data)
        return;

    if (data->segments) {
        for (i = 0; i < data->segment_count; i++) {
            /* note that the close_streamfile won't do anything but deallocate itself,
             * there is only one open file in vgmstream->ch[0].streamfile */
            close_vgmstream(data->segments[i]);
        }
        free(data->segments);
    }
    free(data);
}

void reset_layout_segmented(segmented_layout_data *data) {
    int i;

    if (!data)
        return;

    data->current_segment = 0;
    for (i = 0; i < data->segment_count; i++) {
        reset_vgmstream(data->segments[i]);
    }
}
