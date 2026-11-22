/*****************************************************************
|
|    AP4 - Sample Timing Test
|
|    This file is part of Bento4/AP4 (MP4 Atom Processing Library).
|
 ****************************************************************/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "Ap4.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define BANNER "Sample Timing Test - Version 1.0\n"\
               "(Bento4 Version " AP4_VERSION_STRING ")\n"

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
#define CHECK(x) do { \
    if (!(x)) { fprintf(stderr, "ERROR line %d: %s\n", __LINE__, #x); return 1; }\
} while (0)

/*----------------------------------------------------------------------
|   test description of the reference 6-frame intro
|
|   media timescale 24000, movie timescale 1000, 6 frames of 1001,
|   sizes 40/18/22/55/16/27, composition offsets
|   2002/0/1001/-1001/3003/0, frames 1 and 5 are sync samples.
+---------------------------------------------------------------------*/
static const unsigned int kFrameCount = 6;
static const AP4_UI32     kMediaTimescale = 24000;
static const AP4_UI32     kMovieTimescale = 1000;
static const AP4_UI32     kFrameDuration = 1001;
static const AP4_UI64     kMediaDuration = 6006;
static const unsigned int kSizes[kFrameCount] = {40, 18, 22, 55, 16, 27};
static const AP4_SI32     kCtsDeltas[kFrameCount] = {2002, 0, 1001, -1001, 3003, 0};
static const bool         kSync[kFrameCount] = {true, false, false, false, true, false};

// the fragment carries its own sizes and composition offsets: the first 4
// samples have explicit sizes and offsets, the last 2 use the defaults
static const unsigned int kFragmentSizes[kFrameCount] = {10, 14, 9, 21, 8, 8};
static const AP4_SI32     kFragmentCtsDeltas[kFrameCount] = {2002, 0, -1001, 3003, 0, 0};
// in a fragment, only the first sample carries a sync flag
static const bool         kFragmentSync[kFrameCount] = {true, false, false, false, false, false};

/*----------------------------------------------------------------------
|   PrintUsageAndExit
+---------------------------------------------------------------------*/
static void
PrintUsageAndExit()
{
    fprintf(stderr,
            BANNER
            "\n\nusage: sampletimingtest [h264-annex-b-source.mp4]\n");
    exit(1);
}

/*----------------------------------------------------------------------
|   CheckSamples
+---------------------------------------------------------------------*/
static int
CheckWholeFileSamples(AP4_SampleTable* table, AP4_UI64 dts_origin)
{
    CHECK(table != NULL);
    CHECK(table->GetSampleCount() == kFrameCount);

    for (AP4_Ordinal i=0; i<kFrameCount; i++) {
        AP4_Sample sample;
        CHECK(AP4_SUCCEEDED(table->GetSample(i, sample)));

        // decoding time
        AP4_UI64 expected_dts = dts_origin+(AP4_UI64)i*kFrameDuration;
        CHECK(sample.GetDts() == expected_dts);

        // duration and size
        CHECK(sample.GetDuration() == kFrameDuration);
        CHECK(sample.GetSize() == kSizes[i]);

        // unsigned and signed composition offsets
        CHECK(sample.GetCtsDelta() == (AP4_UI32)kCtsDeltas[i]);
        CHECK(sample.GetCtsDeltaSigned() == kCtsDeltas[i]);

        // composition time
        AP4_UI64 expected_cts = expected_dts+(AP4_SI64)kCtsDeltas[i];
        CHECK(sample.GetCts() == expected_cts);

        // sync flag
        CHECK(sample.IsSync() == kSync[i]);
    }

    return 0;
}

/*----------------------------------------------------------------------
|   CheckFragmentSamples
+---------------------------------------------------------------------*/
static int
CheckFragmentSamples(AP4_SampleTable* table, AP4_UI64 dts_origin)
{
    CHECK(table != NULL);
    CHECK(table->GetSampleCount() == kFrameCount);

    for (AP4_Ordinal i=0; i<kFrameCount; i++) {
        AP4_Sample sample;
        CHECK(AP4_SUCCEEDED(table->GetSample(i, sample)));

        // decoding time
        AP4_UI64 expected_dts = dts_origin+(AP4_UI64)i*kFrameDuration;
        CHECK(sample.GetDts() == expected_dts);

        // duration and size (the last 2 samples use the default size)
        CHECK(sample.GetDuration() == kFrameDuration);
        CHECK(sample.GetSize() == kFragmentSizes[i]);

        // unsigned and signed composition offsets (the last 2 samples have none)
        CHECK(sample.GetCtsDelta() == (AP4_UI32)kFragmentCtsDeltas[i]);
        CHECK(sample.GetCtsDeltaSigned() == kFragmentCtsDeltas[i]);

        // composition time
        AP4_UI64 expected_cts = expected_dts+(AP4_SI64)kFragmentCtsDeltas[i];
        CHECK(sample.GetCts() == expected_cts);

        // sync flag and description index
        CHECK(sample.IsSync() == kFragmentSync[i]);
        CHECK(sample.GetDescriptionIndex() == 1);
    }

    return 0;
}

/*----------------------------------------------------------------------
|   CheckOffsets
+---------------------------------------------------------------------*/
static int
CheckOffsets(AP4_SampleTable* table, AP4_Position first_offset)
{
    AP4_Position expected = first_offset;
    for (AP4_Ordinal i=0; i<kFrameCount; i++) {
        AP4_Sample sample;
        CHECK(AP4_SUCCEEDED(table->GetSample(i, sample)));
        CHECK(sample.GetOffset() == expected);
        expected += sample.GetSize();
    }
    return 0;
}

/*----------------------------------------------------------------------
|   WholeFileTest
+---------------------------------------------------------------------*/
static int
WholeFileTest()
{
    // create a byte stream with enough space for all the sample data
    AP4_ByteStream* data_stream = NULL;
    CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing-data.bin",
                                                   AP4_FileByteStream::STREAM_MODE_WRITE,
                                                   data_stream)));
    unsigned char buffer[512];
    for (unsigned int i=0; i<sizeof(buffer); i++) buffer[i] = (unsigned char)i;
    CHECK(AP4_SUCCEEDED(data_stream->Write(buffer, sizeof(buffer))));
    data_stream->Release();

    AP4_ByteStream* input = NULL;
    CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing-data.bin",
                                                   AP4_FileByteStream::STREAM_MODE_READ,
                                                   input)));

    // build a synthetic sample table with the reference timing
    AP4_SyntheticSampleTable* sample_table = new AP4_SyntheticSampleTable();
    sample_table->AddSampleDescription(
        new AP4_GenericVideoSampleDescription(AP4_SAMPLE_FORMAT_AVC1,
                                              160, 120, 24, "test", NULL),
        true);
    AP4_Position offset = 0;
    for (unsigned int i=0; i<kFrameCount; i++) {
        CHECK(AP4_SUCCEEDED(sample_table->AddSample(*input,
                                                    offset,
                                                    kSizes[i],
                                                    kFrameDuration,
                                                    0,
                                                    (AP4_UI64)i*kFrameDuration,
                                                    (AP4_UI32)kCtsDeltas[i],
                                                    kSync[i])));
        offset += kSizes[i];
    }

    // create a movie with the reference timing and write it out
    AP4_Movie* movie = new AP4_Movie(kMovieTimescale, kMediaDuration);
    movie->AddTrack(new AP4_Track(AP4_Track::TYPE_VIDEO,
                                  sample_table,
                                  1,
                                  kMovieTimescale,
                                  kMediaDuration,
                                  kMediaTimescale,
                                  kMediaDuration,
                                  "und",
                                  0,
                                  0));
    AP4_File file(movie);
    AP4_ByteStream* output = NULL;
    CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing.mp4",
                                                   AP4_FileByteStream::STREAM_MODE_WRITE,
                                                   output)));
    CHECK(AP4_SUCCEEDED(AP4_FileWriter::Write(file, *output)));
    output->Release();
    input->Release();

    // read it back and check every field
    AP4_ByteStream* readback = NULL;
    CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing.mp4",
                                                   AP4_FileByteStream::STREAM_MODE_READ,
                                                   readback)));
    AP4_File* parsed = new AP4_File(*readback);
    AP4_Movie* parsed_movie = parsed->GetMovie();
    CHECK(parsed_movie != NULL);
    AP4_Track* track = parsed_movie->GetTrack(1);
    CHECK(track != NULL);
    CHECK(track->GetMediaTimeScale() == kMediaTimescale);

    AP4_SampleTable* parsed_table = track->GetSampleTable();
    CHECK(CheckWholeFileSamples(parsed_table, 0) == 0);
    CHECK(CheckOffsets(parsed_table, 695) == 0);

    // lookup by decoding time
    for (AP4_Ordinal i=0; i<kFrameCount; i++) {
        AP4_Ordinal index = 999;
        CHECK(AP4_SUCCEEDED(parsed_table->GetSampleIndexForTimeStamp(i*kFrameDuration, index)));
        CHECK(index == i);
    }

    // nearest sync sample, looking before
    CHECK(parsed_table->GetNearestSyncSampleIndex(4, true) == 0);
    CHECK(parsed_table->GetNearestSyncSampleIndex(5, true) == 4);

    // set one sample's composition time and check the delta semantics
    {
        AP4_Sample sample;
        CHECK(AP4_SUCCEEDED(parsed_table->GetSample(3, sample)));
        CHECK(sample.GetDts() == 3003);

        // setting a CTS before the DTS is clamped to the DTS
        sample.SetCts(2002);
        CHECK(sample.GetCtsDelta() == 0);
        CHECK(sample.GetCts() == 3003);

        // setting a later CTS gives a positive delta
        sample.SetCts(5005);
        CHECK(sample.GetCtsDelta() == 2002);
        CHECK(sample.GetCts() == 5005);

        // a negative delta can be expressed either way
        sample.SetCtsDelta((AP4_UI32)-1001);
        CHECK(sample.GetCtsDelta() == 4294966295UL);
        CHECK(sample.GetCtsDeltaSigned() == -1001);
        CHECK(sample.GetCts() == 2002);

        sample.SetCtsDeltaSigned(-1001);
        CHECK(sample.GetCtsDelta() == 4294966295UL);
        CHECK(sample.GetCts() == 2002);
    }

    delete parsed;
    readback->Release();

    return 0;
}

/*----------------------------------------------------------------------
|   BuildFragmentSampleTable
|
|   Build a fragment with the reference timing, either with the data
|   offset equal to the fragment start (samples start at the media
|   payload), or with an explicit data offset split into two trun atoms
|   that both point to the same bytes.
+---------------------------------------------------------------------*/
static AP4_FragmentSampleTable*
BuildFragmentSampleTable(int             mode, // 0: no data offset, 1: data offset 0, 2: two truns
                         AP4_Position    moof_offset,
                         AP4_Position    payload_offset,
                         AP4_ByteStream* stream)
{
    // default sample duration, size, flags and description index
    AP4_ContainerAtom traf(AP4_ATOM_TYPE_TRAF);
    AP4_TfhdAtom* tfhd = new AP4_TfhdAtom(
        AP4_TFHD_FLAG_DEFAULT_BASE_IS_MOOF |
        AP4_TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT |
        AP4_TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT |
        AP4_TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT |
        AP4_TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT,
        1,             // track id
        0,             // base data offset
        2,             // sample description index
        kFrameDuration,
        8,             // default sample size
        0x1010000);    // non sync sample flags
    traf.AddChild(tfhd);

    AP4_TfdtAtom* tfdt = new AP4_TfdtAtom(1, 48000);
    traf.AddChild(tfdt);

    if (mode == 0) {
        // one trun with all 6 entries and no data offset field: the samples
        // start at the media payload of the fragment
        AP4_TrunAtom* trun = new AP4_TrunAtom(
            AP4_TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT |
            AP4_TRUN_FLAG_SAMPLE_SIZE_PRESENT |
            AP4_TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT,
            0,
            0x2000000); // first sample is an I frame
        AP4_Array<AP4_TrunAtom::Entry> entries;
        entries.SetItemCount(kFrameCount);
        for (unsigned int i=0; i<kFrameCount; i++) {
            entries[i].sample_size = kFragmentSizes[i];
            entries[i].sample_composition_time_offset = (AP4_UI32)kFragmentCtsDeltas[i];
        }
        trun->SetEntries(entries);
        traf.AddChild(trun);
    } else if (mode == 1) {
        // one trun with a data offset of 0: because the data offset is equal
        // to the fragment start, the samples start at the media payload too
        AP4_TrunAtom* trun = new AP4_TrunAtom(
            AP4_TRUN_FLAG_DATA_OFFSET_PRESENT |
            AP4_TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT |
            AP4_TRUN_FLAG_SAMPLE_SIZE_PRESENT |
            AP4_TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT,
            0,
            0x2000000);
        AP4_Array<AP4_TrunAtom::Entry> entries;
        entries.SetItemCount(kFrameCount);
        for (unsigned int i=0; i<kFrameCount; i++) {
            entries[i].sample_size = kFragmentSizes[i];
            entries[i].sample_composition_time_offset = (AP4_UI32)kFragmentCtsDeltas[i];
        }
        trun->SetEntries(entries);
        traf.AddChild(trun);
    } else {
        // first trun: sizes and composition offsets for the first 4 samples
        AP4_TrunAtom* trun = new AP4_TrunAtom(
            AP4_TRUN_FLAG_DATA_OFFSET_PRESENT |
            AP4_TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT |
            AP4_TRUN_FLAG_SAMPLE_SIZE_PRESENT |
            AP4_TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT,
            32,
            0x2000000);
        AP4_Array<AP4_TrunAtom::Entry> entries;
        entries.SetItemCount(4);
        for (unsigned int i=0; i<4; i++) {
            entries[i].sample_size = kFragmentSizes[i];
            entries[i].sample_composition_time_offset = (AP4_UI32)kFragmentCtsDeltas[i];
        }
        trun->SetEntries(entries);
        traf.AddChild(trun);

        // second trun: only a data offset, back to the same bytes
        AP4_TrunAtom* trun2 = new AP4_TrunAtom(
            AP4_TRUN_FLAG_DATA_OFFSET_PRESENT,
            32,
            0);
        AP4_Array<AP4_TrunAtom::Entry> entries2;
        entries2.SetItemCount(2);
        trun2->SetEntries(entries2);
        traf.AddChild(trun2);
    }

    AP4_FragmentSampleTable* table = new AP4_FragmentSampleTable(&traf,
                                                                 NULL,
                                                                 stream,
                                                                 moof_offset,
                                                                 payload_offset,
                                                                 0);
    return table;
}

/*----------------------------------------------------------------------
|   FragmentTest
+---------------------------------------------------------------------*/
static int
FragmentTest()
{
    AP4_ByteStream* input = NULL;
    CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing-data.bin",
                                                   AP4_FileByteStream::STREAM_MODE_READ,
                                                   input)));

    // case A: no data offset field -> samples start at the media payload
    {
        AP4_FragmentSampleTable* table = BuildFragmentSampleTable(0, 1800, 1800, input);
        CHECK(CheckFragmentSamples(table, 48000) == 0);
        CHECK(CheckOffsets(table, 1800) == 0);
        CHECK(table->GetDuration() == kMediaDuration);
        for (AP4_Ordinal i=0; i<kFrameCount; i++) {
            AP4_Sample sample;
            CHECK(AP4_SUCCEEDED(table->GetSample(i, sample)));
            CHECK(sample.GetDescriptionIndex() == 1);
        }

        // lookup by decoding time
        for (AP4_Ordinal i=0; i<kFrameCount; i++) {
            AP4_Ordinal index = 999;
            CHECK(AP4_SUCCEEDED(table->GetSampleIndexForTimeStamp(48000+i*kFrameDuration, index)));
            CHECK(index == i);
        }

        // nearest sync sample, looking before (only the first fragment sample is a sync sample)
        CHECK(table->GetNearestSyncSampleIndex(4, true) == 0);
        CHECK(table->GetNearestSyncSampleIndex(5, true) == 0);
        CHECK(table->GetNearestSyncSampleIndex(5, false) == kFrameCount);

        delete table;
    }

    // case B: a data offset equal to the fragment start also means that the
    // samples start at the media payload
    {
        AP4_FragmentSampleTable* table = BuildFragmentSampleTable(1, 1800, 1800, input);
        CHECK(CheckFragmentSamples(table, 48000) == 0);
        CHECK(CheckOffsets(table, 1800) == 0);
        delete table;
    }

    // case C: explicit data offset of 32 from a fragment at 5000, split in
    // two trun atoms that both point at the same bytes
    {
        AP4_FragmentSampleTable* table = BuildFragmentSampleTable(2, 5000, 5032, input);
        CHECK(CheckFragmentSamples(table, 48000) == 0);
        CHECK(table->GetDuration() == kMediaDuration);
        const AP4_Position expected_offsets[kFrameCount] = {
            5032, 5042, 5056, 5065, 5032, 5040
        };
        for (AP4_Ordinal i=0; i<kFrameCount; i++) {
            AP4_Sample sample;
            CHECK(AP4_SUCCEEDED(table->GetSample(i, sample)));
            CHECK(sample.GetOffset() == expected_offsets[i]);
        }
        delete table;
    }

    input->Release();
    return 0;
}

/*----------------------------------------------------------------------
|   CaptureSampleSet
+---------------------------------------------------------------------*/
struct CaptureSample {
    std::vector<unsigned char> data;
};

/*----------------------------------------------------------------------
|   SplitLengthPrefixed
+---------------------------------------------------------------------*/
static int
SplitLengthPrefixed(const AP4_DataBuffer& buffer,
                    std::vector<std::vector<unsigned char> >& nal_units)
{
    const unsigned char* data = buffer.GetData();
    AP4_Size             left = buffer.GetDataSize();
    while (left >= 4) {
        AP4_UI32 size = AP4_BytesToUInt32BE(data);
        data += 4;
        left -= 4;
        if (size == 0 || size > left) return 1;
        nal_units.push_back(std::vector<unsigned char>(data, data+size));
        data += size;
        left -= size;
    }
    return left == 0 ? 0 : 1;
}

/*----------------------------------------------------------------------
|   ReadTrackSamples
+---------------------------------------------------------------------*/
static int
ReadTrackSamples(const char* filename,
                 std::vector<CaptureSample>& samples,
                 AP4_UI32& timescale,
                 AP4_Array<AP4_DataBuffer>& sps_list,
                 AP4_Array<AP4_DataBuffer>& pps_list)
{
    AP4_ByteStream* input = NULL;
    if (AP4_FAILED(AP4_FileByteStream::Create(filename,
                                              AP4_FileByteStream::STREAM_MODE_READ,
                                              input))) {
        return 1;
    }
    AP4_File file(*input);
    AP4_Movie* movie = file.GetMovie();
    if (movie == NULL) { input->Release(); return 1; }
    AP4_Track* track = movie->GetTrack(AP4_Track::TYPE_VIDEO);
    if (track == NULL) { input->Release(); return 1; }
    timescale = track->GetMediaTimeScale();

    AP4_AvcSampleDescription* desc =
        AP4_DYNAMIC_CAST(AP4_AvcSampleDescription, track->GetSampleDescription(0));
    if (desc == NULL) { input->Release(); return 1; }
    sps_list = desc->GetSequenceParameters();
    pps_list = desc->GetPictureParameters();

    for (AP4_Ordinal i=0; i<track->GetSampleCount(); i++) {
        AP4_Sample sample;
        AP4_DataBuffer data;
        if (AP4_FAILED(track->ReadSample(i, sample, data))) {
            input->Release();
            return 1;
        }
        CaptureSample captured;
        captured.data.assign(data.GetData(), data.GetData()+data.GetDataSize());
        samples.push_back(captured);
    }

    input->Release();
    return 0;
}

/*----------------------------------------------------------------------
|   MakeAnnexBStream
|
|   Convert length-prefixed sample data to a start-code delimited stream.
|   Parameter sets are prepended, as a capture device would deliver them.
+---------------------------------------------------------------------*/
static void
MakeAnnexBStream(const std::vector<CaptureSample>&      samples,
                 const AP4_Array<AP4_DataBuffer>&       sps_list,
                 const AP4_Array<AP4_DataBuffer>&       pps_list,
                 unsigned int                           start_code_size,
                 std::vector<unsigned char>&            stream,
                 std::vector<std::vector<unsigned char> >& expected_nals)
{
    for (unsigned int i=0; i<sps_list.ItemCount(); i++) {
        for (unsigned int j=0; j<start_code_size-1; j++) stream.push_back(0);
        stream.push_back(1);
        stream.insert(stream.end(), sps_list[i].GetData(),
                      sps_list[i].GetData()+sps_list[i].GetDataSize());
    }
    for (unsigned int i=0; i<pps_list.ItemCount(); i++) {
        for (unsigned int j=0; j<start_code_size-1; j++) stream.push_back(0);
        stream.push_back(1);
        stream.insert(stream.end(), pps_list[i].GetData(),
                      pps_list[i].GetData()+pps_list[i].GetDataSize());
        expected_nals.push_back(std::vector<unsigned char>(pps_list[i].GetData(),
                                                          pps_list[i].GetData()+pps_list[i].GetDataSize()));
    }
    for (unsigned int i=0; i<samples.size(); i++) {
        std::vector<std::vector<unsigned char> > nals;
        AP4_DataBuffer buffer(samples[i].data.empty()?NULL:&samples[i].data[0],
                              (AP4_Size)samples[i].data.size());
        if (SplitLengthPrefixed(buffer, nals)) continue;
        for (unsigned int j=0; j<nals.size(); j++) {
            for (unsigned int k=0; k<start_code_size-1; k++) stream.push_back(0);
            stream.push_back(1);
            stream.insert(stream.end(), nals[j].begin(), nals[j].end());
        }
    }
}

/*----------------------------------------------------------------------
|   FeedCapture
+---------------------------------------------------------------------*/
static AP4_Result
FeedCapture(AP4_AvcSegmentBuilder&        builder,
            const std::vector<unsigned char>& stream,
            AP4_Size                      chunk_size)
{
    AP4_Size offset = 0;
    while (offset < stream.size()) {
        AP4_Size chunk = (AP4_Size)stream.size()-offset;
        if (chunk > chunk_size) chunk = chunk_size;
        AP4_Size consumed = 0;
        AP4_Result result = builder.Feed(&stream[offset], chunk, consumed);
        if (result < 0) return result;
        if (consumed == 0) return AP4_ERROR_INTERNAL;
        offset += consumed;
    }
    AP4_Size consumed = 0;
    return builder.Feed(NULL, 0, consumed);
}

/*----------------------------------------------------------------------
|   CaptureNalUnitList
+---------------------------------------------------------------------*/
static int
CaptureNalUnitList(AP4_Sample& sample, std::vector<std::vector<unsigned char> >& nal_units)
{
    AP4_DataBuffer data;
    if (AP4_FAILED(sample.ReadData(data))) return 1;
    return SplitLengthPrefixed(data, nal_units);
}

/*----------------------------------------------------------------------
|   CaptureTest
|
|   Check that:
|   - the access unit boundaries are the same for 3-byte and 4-byte start
|     codes, and for any feed chunk size
|   - the NAL units are the ones that were captured, in the same order
|   - the packaged byte positions are the cumulative sizes
|   - a 30fps capture gives a 30000 timescale and 1000 per frame
+---------------------------------------------------------------------*/
static int
CaptureTest(const char* h264_filename)
{
    std::vector<CaptureSample> samples;
    AP4_UI32 timescale = 0;
    AP4_Array<AP4_DataBuffer> sps_list;
    AP4_Array<AP4_DataBuffer> pps_list;
    if (ReadTrackSamples(h264_filename, samples, timescale, sps_list, pps_list)) {
        printf("INFO: no H.264 test input (%s), capture test skipped\n", h264_filename);
        return 0;
    }
    if (samples.empty()) {
        printf("INFO: no samples in H.264 test input, capture test skipped\n");
        return 0;
    }

    // reference NAL units for each captured sample
    std::vector<std::vector<std::vector<unsigned char> > > reference;
    for (unsigned int i=0; i<samples.size(); i++) {
        std::vector<std::vector<unsigned char> > nals;
        AP4_DataBuffer buffer(samples[i].data.empty()?NULL:&samples[i].data[0],
                              (AP4_Size)samples[i].data.size());
        if (SplitLengthPrefixed(buffer, nals)) continue;
        reference.push_back(nals);
    }

    const AP4_Size chunk_sizes[] = {64, 1024, 65536};
    for (unsigned int sc=3; sc<=4; sc += 1) {
        for (unsigned int ci=0; ci<sizeof(chunk_sizes)/sizeof(chunk_sizes[0]); ci++) {
            std::vector<unsigned char> stream;
            std::vector<std::vector<unsigned char> > prefix_nals;
            MakeAnnexBStream(samples, sps_list, pps_list, sc, stream, prefix_nals);

            AP4_AvcSegmentBuilder builder(1, 30.0);
            CHECK(FeedCapture(builder, stream, chunk_sizes[ci]) >= 0);

            AP4_Array<AP4_Sample>& built = builder.GetSamples();
            CHECK(built.ItemCount() == samples.size());
            CHECK(builder.GetTimescale() == 30000);
            CHECK(builder.GetMediaDuration() == (AP4_UI64)samples.size()*1000);

            // keep a copy of the samples, because WriteMediaSegment() resets
            // the builder's sample list
            std::vector<AP4_Size>     built_sizes;
            std::vector<AP4_UI64>     built_dts;
            std::vector<AP4_UI32>     built_durations;
            for (unsigned int i=0; i<built.ItemCount(); i++) {
                built_sizes.push_back(built[i].GetSize());
                built_dts.push_back(built[i].GetDts());
                built_durations.push_back(built[i].GetDuration());
            }

            for (unsigned int i=0; i<built.ItemCount(); i++) {
                // the times are 1000 per frame, starting at 0
                CHECK(built[i].GetDts() == (AP4_UI64)i*1000);
                CHECK(built[i].GetDuration() == 1000);

                // the NAL units are the same, in the same order, and the
                // only extra units are the parameter sets on the first sample
                std::vector<std::vector<unsigned char> > nals;
                CHECK(CaptureNalUnitList(built[i], nals) == 0);
                unsigned int prefix_count = (i == 0) ? (unsigned int)prefix_nals.size() : 0;
                CHECK(nals.size() == reference[i].size()+prefix_count);
                for (unsigned int k=0; k<nals.size(); k++) {
                    if (prefix_count && k < prefix_count) {
                        CHECK(nals[k] == prefix_nals[k]);
                    } else {
                        CHECK(nals[k] == reference[i][k-prefix_count]);
                    }
                }

                // the sample size is the sum of the length-prefixed NAL units
                AP4_Size expected_size = 0;
                for (unsigned int k=0; k<nals.size(); k++) {
                    expected_size += 4+(AP4_Size)nals[k].size();
                }
                CHECK(built[i].GetSize() == expected_size);
            }

            // write the captured samples as a whole file and read it back:
            // the byte positions must be the cumulative sizes of the samples
            // built by the capture path, with the first sample holding the
            // parameter sets and no other sample carrying them
            AP4_SyntheticSampleTable* whole_table = new AP4_SyntheticSampleTable();
            whole_table->AddSampleDescription(
                new AP4_GenericVideoSampleDescription(AP4_SAMPLE_FORMAT_AVC1,
                                                      160, 120, 24, "capture", NULL),
                true);
            for (AP4_Ordinal i=0; i<built.ItemCount(); i++) {
                AP4_ByteStream* data_stream = built[i].GetDataStream();
                CHECK(data_stream != NULL);
                CHECK(AP4_SUCCEEDED(whole_table->AddSample(*data_stream,
                                                           built[i].GetOffset(),
                                                           built[i].GetSize(),
                                                           built[i].GetDuration(),
                                                           0,
                                                           built[i].GetDts(),
                                                           built[i].GetCtsDelta(),
                                                           built[i].IsSync())));
                AP4_RELEASE(data_stream);
            }
            AP4_Movie* whole_movie = new AP4_Movie(1000, builder.GetMediaDuration()/30);
            whole_movie->AddTrack(new AP4_Track(AP4_Track::TYPE_VIDEO,
                                                whole_table,
                                                1,
                                                1000,
                                                builder.GetMediaDuration()/30,
                                                builder.GetTimescale(),
                                                builder.GetMediaDuration(),
                                                "und",
                                                0,
                                                0));
            AP4_File whole_file(whole_movie);
            AP4_ByteStream* whole_output = NULL;
            CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing-capture.mp4",
                                                           AP4_FileByteStream::STREAM_MODE_WRITE,
                                                           whole_output)));
            CHECK(AP4_SUCCEEDED(AP4_FileWriter::Write(whole_file, *whole_output)));
            whole_output->Release();

            // write a media segment and read the samples back
            AP4_ByteStream* segment_stream = NULL;
            CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing-capture.m4s",
                                                           AP4_FileByteStream::STREAM_MODE_WRITE,
                                                           segment_stream)));
            CHECK(AP4_SUCCEEDED(builder.WriteMediaSegment(*segment_stream, 1)));
            segment_stream->Release();

            AP4_ByteStream* init_stream = NULL;
            CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing-capture-init.mp4",
                                                           AP4_FileByteStream::STREAM_MODE_WRITE,
                                                           init_stream)));
            CHECK(AP4_SUCCEEDED(builder.WriteInitSegment(*init_stream)));
            init_stream->Release();

            // parse the init segment and the media segment back
            AP4_ByteStream* init_read = NULL;
            CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing-capture-init.mp4",
                                                           AP4_FileByteStream::STREAM_MODE_READ,
                                                           init_read)));
            AP4_File init_file(*init_read, true);
            AP4_Movie* init_movie = init_file.GetMovie();
            CHECK(init_movie != NULL);

            AP4_ByteStream* media_read = NULL;
            CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing-capture.m4s",
                                                           AP4_FileByteStream::STREAM_MODE_READ,
                                                           media_read)));
            AP4_Position moof_offset = 0;
            media_read->Tell(moof_offset);
            AP4_DefaultAtomFactory atom_factory;
            AP4_Atom* atom = NULL;
            CHECK(AP4_SUCCEEDED(atom_factory.CreateAtomFromStream(*media_read, atom)));
            AP4_Position after_moof = 0;
            media_read->Tell(after_moof);
            AP4_ContainerAtom* moof = AP4_DYNAMIC_CAST(AP4_ContainerAtom, atom);
            CHECK(moof != NULL);

            AP4_MovieFragment fragment(moof);
            AP4_FragmentSampleTable* table = NULL;
            CHECK(AP4_SUCCEEDED(fragment.CreateSampleTable(init_movie,
                                                           1,
                                                           media_read,
                                                           moof_offset,
                                                           after_moof+8,
                                                           0,
                                                           table)));
            CHECK(table->GetSampleCount() == samples.size());
            AP4_Position expected_offset = after_moof+8;
            for (AP4_Ordinal i=0; i<table->GetSampleCount(); i++) {
                AP4_Sample sample;
                CHECK(AP4_SUCCEEDED(table->GetSample(i, sample)));
                CHECK(sample.GetDts() == (AP4_UI64)i*1000);
                CHECK(sample.GetDuration() == 1000);
                CHECK(sample.GetSize() == built_sizes[i]);
                CHECK(sample.GetOffset() == expected_offset);
                expected_offset += sample.GetSize();
            }
            delete table;
            media_read->Release();
            init_read->Release();

            AP4_ByteStream* whole_read = NULL;
            CHECK(AP4_SUCCEEDED(AP4_FileByteStream::Create("/tmp/ap4-sample-timing-capture.mp4",
                                                           AP4_FileByteStream::STREAM_MODE_READ,
                                                           whole_read)));
            AP4_File* whole_parsed = new AP4_File(*whole_read);
            AP4_Track* whole_track = whole_parsed->GetMovie()->GetTrack(1);
            CHECK(whole_track != NULL);
            AP4_Position whole_offset = 0;
            // skip to the first sample's offset, then the samples are contiguous
            AP4_Sample first_sample;
            CHECK(AP4_SUCCEEDED(whole_track->GetSample(0, first_sample)));
            whole_offset = first_sample.GetOffset();
            for (AP4_Ordinal i=0; i<whole_track->GetSampleCount(); i++) {
                AP4_Sample sample;
                CHECK(AP4_SUCCEEDED(whole_track->GetSample(i, sample)));
                CHECK(sample.GetSize() == built_sizes[i]);
                CHECK(sample.GetOffset() == whole_offset);
                CHECK(sample.GetDts() == built_dts[i]);
                CHECK(sample.GetDuration() == built_durations[i]);
                whole_offset += sample.GetSize();
            }
            delete whole_parsed;
            whole_read->Release();
        }
    }

    return 0;
}

/*----------------------------------------------------------------------
|   HevcDurationTest
|
|   The HEVC packaging path shares the access-unit packaging code with
|   the AVC path, so the frame duration for a given frame rate must be
|   the same. Expose the shared method to check it directly, since a
|   synthetic HEVC bitstream is not needed for the timing computation.
+---------------------------------------------------------------------*/
class ExposedHevcSegmentBuilder : public AP4_HevcSegmentBuilder
{
public:
    ExposedHevcSegmentBuilder(AP4_UI32 track_id, double fps) :
        AP4_HevcSegmentBuilder(track_id, fps) {}

    AP4_Result AddTestAccessUnit(bool is_sync,
                                 AP4_UI32 decode_order,
                                 AP4_UI32 display_order)
    {
        AP4_Array<AP4_DataBuffer*> nal_units;
        nal_units.Append(new AP4_DataBuffer((const AP4_Byte*) "\x00\x00\x00\x02\x40\x01", 6));
        AP4_Result result = AddAccessUnit(nal_units, is_sync, decode_order, display_order);
        for (unsigned int i=0; i<nal_units.ItemCount(); i++) delete nal_units[i];
        return result;
    }
};

static int
HevcDurationTest()
{
    ExposedHevcSegmentBuilder builder(1, 30.0);
    for (unsigned int i=0; i<6; i++) {
        CHECK(AP4_SUCCEEDED(builder.AddTestAccessUnit(i == 0, i, i)));
    }
    CHECK(builder.GetTimescale() == 30000);
    CHECK(builder.GetSamples().ItemCount() == 6);
    CHECK(builder.GetMediaDuration() == 6000);
    for (unsigned int i=0; i<6; i++) {
        CHECK(builder.GetSamples()[i].GetDts() == (AP4_UI64)i*1000);
        CHECK(builder.GetSamples()[i].GetDuration() == 1000);
    }

    return 0;
}

/*----------------------------------------------------------------------
|   main
+---------------------------------------------------------------------*/
int
main(int argc, char** argv)
{
    if (argc > 2) PrintUsageAndExit();
    const char* h264_filename = (argc == 2) ? argv[1] : "Test/Data/video-h264-001.mp4";

    printf("whole file...\n");
    if (WholeFileTest()) return 1;

    printf("fragment...\n");
    if (FragmentTest()) return 1;

    printf("hevc frame duration...\n");
    if (HevcDurationTest()) return 1;

    printf("capture...\n");
    if (CaptureTest(h264_filename)) return 1;

    printf("all sample timing tests passed\n");
    return 0;
}
