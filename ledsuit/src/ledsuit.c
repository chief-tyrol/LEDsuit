#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <fftw3.h>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE 32000
#define BUFLEN 8192

int setup_audio(snd_pcm_t **capture_handle, char* name){
	int err;
	unsigned int tmp = SAMPLE_RATE;
	snd_pcm_hw_params_t *hw_params;

	if ((err = snd_pcm_open (capture_handle, name, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf (stderr, "cannot open audio device %s (%s)\n",
				name,
				snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
				snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_any (*capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
				snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_access (*capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n",
				snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_format (*capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n",
				snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_rate_near (*capture_handle, hw_params, &tmp, 0)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",
				snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_channels (*capture_handle, hw_params, 1)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n",
				snd_strerror (err));
		exit (1);
	}


	/* Set period size to 32 frames. */
	snd_pcm_uframes_t frames = 32;//BUFLEN / 5;
	if ((err = snd_pcm_hw_params_set_period_size_near(*capture_handle, hw_params, &frames, 0)) < 0) {
		fprintf (stderr, "Cannot set period size (%s)\n",
				snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params (*capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n",
				snd_strerror (err));
		exit (1);
	}

	snd_pcm_hw_params_free (hw_params);

	if ((err = snd_pcm_prepare (*capture_handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
				snd_strerror (err));
		exit (1);
	}

	return 0;
}

int cleanup_audio(snd_pcm_t *capture_handle){
	snd_pcm_drain(capture_handle);
	snd_pcm_close (capture_handle);
	return 0;
}



int main(int argc, char**argv) {
	snd_pcm_t *capture_handle;
	int16_t buf[BUFLEN * 2];
	fftw_plan p;
	double *in;
	fftw_complex *out;
	int loops;

	int i, j, max;
	int err;

	char *name = (argc > 1) ? argv[1] : "default";

	max = (BUFLEN / 2) + 1;
	loops = 15 * SAMPLE_RATE / BUFLEN;

	fprintf(stderr, "Setting up audio with %s device\n", name);
	setup_audio(&capture_handle, name);


	// set up the fftw
	in = (double*) fftw_malloc(sizeof(double) * BUFLEN);
	out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * BUFLEN);

	fprintf(stderr, "Setting up fftw plan... ");
	//fftw_plan fftw_plan_dft_r2c_1d(int n, double *in, fftw_complex *out, unsigned flags);
	p = fftw_plan_dft_r2c_1d(BUFLEN, in, out, FFTW_MEASURE);
	fprintf(stderr, "Done!\n");

	fprintf(stderr, "Starting main loop (%d iterations)...\n", loops);
	// main loop
	for(i = 0; i < loops; i++){

		if ((err = snd_pcm_readi (capture_handle, buf, BUFLEN)) != BUFLEN) {
			fprintf (stderr, "read from audio interface failed (%s)\n",
					snd_strerror (err));
			exit (1);
		}

		for(j = 0; j < BUFLEN; j++){
			in[i] = buf[i];
		}

//		for(j = 0; j < BUFLEN; j++){
//			printf("%d", buf[j]);
//			if(j < BUFLEN - 1){
//				printf(",");
//			}
//		}

		// run the fft
		fftw_execute(p);

		for(j = 0; j < max; j++){
			printf("%.4f", out[j][1]);
			if(j < max - 1){
				printf(",");
			}
		}

		printf("\n");
	}

	fprintf(stderr, "Done with main loop\n");

	fftw_destroy_plan(p);
	fftw_free(in);
	fftw_free(out);

	cleanup_audio(capture_handle);
	return 0;
}
