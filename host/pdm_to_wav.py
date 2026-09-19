#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# SPDX-License-Identifier: GPL-3.0
#
# GNU Radio Python Flow Graph
# Title: PDM to WAV
# Author: hunz
# GNU Radio version: 3.10.9.2

from gnuradio import blocks
import pmt
from gnuradio import digital
from gnuradio import filter
from gnuradio.filter import firdes
from gnuradio import gr
from gnuradio.fft import window
import sys
import signal
from argparse import ArgumentParser
from gnuradio.eng_arg import eng_float, intx
from gnuradio import eng_notation




class pdm_to_wav(gr.top_block):

    def __init__(self):
        gr.top_block.__init__(self, "PDM to WAV", catch_exceptions=True)

        ##################################################
        # Variables
        ##################################################
        self.pdm_srate = pdm_srate = 4800000
        self.decim2 = decim2 = 5
        self.decim1 = decim1 = 6
        self.samp_rate = samp_rate = pdm_srate/(decim1*decim2)
        self.output_fname = output_fname = sys.argv[2]
        self.input_fname = input_fname = sys.argv[1]

        ##################################################
        # Blocks
        ##################################################

        self.low_pass_filter_0_0 = filter.fir_filter_fff(
            decim2,
            firdes.low_pass(
                1,
                (pdm_srate/decim1),
                78e3,
                4e3,
                window.WIN_HAMMING,
                6.76))
        self.low_pass_filter_0 = filter.fir_filter_fff(
            decim1,
            firdes.low_pass(
                1,
                pdm_srate,
                300e3,
                100e3,
                window.WIN_HAMMING,
                6.76))
        self.digital_chunks_to_symbols_xx_0 = digital.chunks_to_symbols_bf([-1.0, 1.0], 1)
        self.blocks_wavfile_sink_0 = blocks.wavfile_sink(
            output_fname,
            1,
            int(samp_rate),
            blocks.FORMAT_WAV,
            blocks.FORMAT_FLOAT,
            False
            )
        self.blocks_unpack_k_bits_bb_0 = blocks.unpack_k_bits_bb(8)
        self.blocks_file_source_0 = blocks.file_source(gr.sizeof_char*1, input_fname, False, 0, 0)
        self.blocks_file_source_0.set_begin_tag(pmt.PMT_NIL)


        ##################################################
        # Connections
        ##################################################
        self.connect((self.blocks_file_source_0, 0), (self.blocks_unpack_k_bits_bb_0, 0))
        self.connect((self.blocks_unpack_k_bits_bb_0, 0), (self.digital_chunks_to_symbols_xx_0, 0))
        self.connect((self.digital_chunks_to_symbols_xx_0, 0), (self.low_pass_filter_0, 0))
        self.connect((self.low_pass_filter_0, 0), (self.low_pass_filter_0_0, 0))
        self.connect((self.low_pass_filter_0_0, 0), (self.blocks_wavfile_sink_0, 0))


    def get_pdm_srate(self):
        return self.pdm_srate

    def set_pdm_srate(self, pdm_srate):
        self.pdm_srate = pdm_srate
        self.set_samp_rate(self.pdm_srate/(self.decim1*self.decim2))
        self.low_pass_filter_0.set_taps(firdes.low_pass(1, self.pdm_srate, 300e3, 100e3, window.WIN_HAMMING, 6.76))
        self.low_pass_filter_0_0.set_taps(firdes.low_pass(1, (self.pdm_srate/self.decim1), 78e3, 4e3, window.WIN_HAMMING, 6.76))

    def get_decim2(self):
        return self.decim2

    def set_decim2(self, decim2):
        self.decim2 = decim2
        self.set_samp_rate(self.pdm_srate/(self.decim1*self.decim2))

    def get_decim1(self):
        return self.decim1

    def set_decim1(self, decim1):
        self.decim1 = decim1
        self.set_samp_rate(self.pdm_srate/(self.decim1*self.decim2))
        self.low_pass_filter_0_0.set_taps(firdes.low_pass(1, (self.pdm_srate/self.decim1), 78e3, 4e3, window.WIN_HAMMING, 6.76))

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate

    def get_output_fname(self):
        return self.output_fname

    def set_output_fname(self, output_fname):
        self.output_fname = output_fname
        self.blocks_wavfile_sink_0.open(self.output_fname)

    def get_input_fname(self):
        return self.input_fname

    def set_input_fname(self, input_fname):
        self.input_fname = input_fname
        self.blocks_file_source_0.open(self.input_fname, False)




def main(top_block_cls=pdm_to_wav, options=None):
    tb = top_block_cls()

    def sig_handler(sig=None, frame=None):
        tb.stop()
        tb.wait()

        sys.exit(0)

    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    tb.start()

    tb.wait()


if __name__ == '__main__':
    main()
