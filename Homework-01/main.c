#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

typedef struct
{
	double real;
	double imag;
} complex_t;

#pragma pack(push, 1)

typedef struct format_s
{
	unsigned int block_size;
	unsigned short audio_format;
	unsigned short channels;
	unsigned int sample_rate;
	unsigned int byte_per_sec;
	unsigned short byte_per_block;
	unsigned short bits_per_sample;
} format_t;

#pragma pack(pop)

typedef struct wav_data_s
{
	unsigned int size;
	short* data;
} wav_data_t;

typedef enum sideband
{
	SIDEBAND_USB = 0,
	SIDEBAND_LSB = 1
} sideband_t;

typedef struct carrier_s
{
	double amplitude;
	double frequency;
	double phase;
} carrier_t;

int g_ssb_use_filter = 0;
double g_audio_bandwidth = 22000.0;

wav_data_t load_sample(const char* path, format_t* out_header);

void release_wav_data(wav_data_t* data);

complex_t complex_add(const complex_t* carrier_a, const complex_t* carrier_b);

complex_t complex_sub(const complex_t* carrier_a, const complex_t* carrier_b);

complex_t complex_mul(const complex_t* carrier_a, const complex_t* carrier_b);

complex_t complex_exp(double theta);

double complex_mag(const complex_t* carrier_a);

int* br_order_malloc(int bit_size);

complex_t* fft_malloc(const double* input, size_t samples, size_t* out_size);

complex_t* ifft_malloc(const complex_t* freq_domain, size_t n);

void print_frequency_domain_to_csv(const char* path, const complex_t* data, size_t size, double fs);

void print_time_domain_to_csv(const char* path, const double* data, size_t size, double fs);

double* am_dsb_lc_malloc(const double* input, size_t samples, double fs, const carrier_t* carrier, double ka);

double* am_dsb_sc_malloc(const double* input, size_t samples, double fs, const carrier_t* carrier);

double* am_ssb_malloc(const double* input, size_t samples, double fs
	, const carrier_t* carrier, sideband_t sideband);

double* detect_envelope_malloc(const double* input, size_t samples);

double get_awgn(void);

void save_as_wav_file(const char* path, const double* input, size_t size, const format_t* format);

double* demodulate_with_envelope_detection_malloc(const double* signal, size_t size
	, const carrier_t* carrier, double ka);

double* demodulate_with_coherent_detection_malloc(const double* signal, size_t size
	, const carrier_t* carrier, double fs);



int main(int argc, char** argv)
{	
	const double PI = acos(-1);

	const int FC = 100000; // 100kHz carrier frequency
	const double AC = 1.0;
	const double KA = 0.8;
	carrier_t rf_carrier = { 0.0, 0.0, 0.0 };
	rf_carrier.frequency = FC;
	rf_carrier.amplitude = AC;
	rf_carrier.phase = 0.0;

	format_t format;
	wav_data_t sample;
	
	complex_t* dsb_lc_fft;
	complex_t* dsb_sc_fft;
	complex_t* ssb_usb_fft;
	complex_t* ssb_lsb_fft;

	double* demodulated_dsb_lc;
	double* demodulated_dsb_sc;
	double* demodulated_ssb_usb;
	double* demodulated_ssb_lsb;

	if (argc < 2)
	{
		sample = load_sample("sample.wav", &format);
	}
	else
	{
		sample = load_sample(argv[1], &format);
	}

	printf("block_size: %d, audio_format: %d, channels: %d,\n"
		"sample_rate: %d, byte per second: %d, byte per block: %d\n"
		"bits per sample: %d\n", format.block_size, format.audio_format, format.channels
		, format.sample_rate, format.byte_per_sec, format.byte_per_block
		, format.bits_per_sample);

	printf("data size: %d\n", sample.size);

	double* input = malloc(sizeof(double) * sample.size);
	double mean = 0.0;
	for (size_t i = 0; i < sample.size; ++i)
	{
		input[i] = (double)sample.data[i] / (double)0x7fff;
		mean += input[i];
	}
	mean = mean / sample.size;

	size_t fft_size;

	system("mkdir out");

	complex_t* input_fft = fft_malloc(input, sample.size, &fft_size);
	print_frequency_domain_to_csv("out/sample.csv", input_fft, fft_size, format.sample_rate);
	const complex_t RE0 = { 0.0, 0.0 };

	// LPF

	for (size_t i = 0; i < fft_size; ++i)
	{
		double freq;
		if (i < (fft_size >> 1))
		{
			freq = i * format.sample_rate / (double)fft_size;
		}
		else
		{
			freq = (fft_size - i) * format.sample_rate / (double)fft_size;
		}
		if (freq > 20000.0)
		{
			input_fft[i] = RE0;
		}
	}
	complex_t* input_fft_ifft = ifft_malloc(input_fft, fft_size);
	free(input_fft);
	for (size_t i = 0; i < sample.size; ++i)
	{
		input[i] = input_fft_ifft[i].real;
	}
	free(input_fft_ifft);
	printf("sample mean: %lf\n", mean);

	carrier_t carrier[4] = { 0, };
	const double SNRs[4] = { 0.0, 30.0, 30.0, 30.0 };
	
	for (size_t i = 0; i < 4; ++i)
	{
		carrier[i].amplitude = AC;
		carrier[i].frequency = FC;
		carrier[i].phase = 0.0;
	}
	carrier[2].frequency = FC - 1;
	carrier[3].phase = 89.0 * PI / 180.0;

	for (size_t i = 0; i < 4; ++i)
	{
		// Modulation
		double* dsb_lc = am_dsb_lc_malloc(input, sample.size, format.sample_rate, &rf_carrier, KA);
		double* dsb_sc = am_dsb_sc_malloc(input, sample.size, format.sample_rate, &rf_carrier);
		double* ssb_usb = am_ssb_malloc(input, sample.size, format.sample_rate, &rf_carrier, SIDEBAND_USB);
		double* ssb_lsb = am_ssb_malloc(input, sample.size, format.sample_rate, &rf_carrier, SIDEBAND_LSB);
		double dsb_lc_p = 0.0;
		double dsb_sc_p = 0.0;
		double ssb_usb_p = 0.0;
		double ssb_lsb_p = 0.0;
		for (size_t j = 0; j < sample.size; ++j)
		{
			dsb_lc_p += dsb_lc[j] * dsb_lc[j];
			dsb_sc_p += dsb_sc[j] * dsb_sc[j];
			ssb_usb_p += ssb_usb[j] * ssb_usb[j];
			ssb_lsb_p += ssb_lsb[j] * ssb_lsb[j];
		}
		dsb_lc_p = dsb_lc_p / sample.size;
		dsb_sc_p = dsb_sc_p / sample.size;
		ssb_usb_p  = ssb_usb_p / sample.size;
		ssb_lsb_p = ssb_lsb_p / sample.size;

		double power = pow(10.0, 0.1 * SNRs[i]);
		double dsb_lc_np = sqrt(dsb_lc_p / power);
		double dsb_sc_np = sqrt(dsb_sc_p / power);
		double ssb_usb_np = sqrt(ssb_usb_p / power);
		double ssb_lsb_np = sqrt(ssb_lsb_p / power);	

		srand(0);
		for (size_t j = 0; j < sample.size; ++j)
		{
			double noise = get_awgn();
			dsb_lc[j] += noise * dsb_lc_np;
			dsb_sc[j] += noise * dsb_sc_np;
			ssb_usb[j] += noise * ssb_usb_np;
			ssb_lsb[j] += noise * ssb_lsb_np;
		}


		size_t fft_size;
		const char* channels = "abcd";
		char path[0x20];

// lb_save_freq_domain:

		dsb_lc_fft = fft_malloc(dsb_lc, sample.size, &fft_size);
		dsb_sc_fft = fft_malloc(dsb_sc, sample.size, &fft_size);
		ssb_usb_fft = fft_malloc(ssb_usb, sample.size, &fft_size);
		ssb_lsb_fft = fft_malloc(ssb_lsb, sample.size, &fft_size);

		sprintf(path, "out/%c_dsb-lc.csv", channels[i]);
		print_frequency_domain_to_csv(path, dsb_lc_fft, fft_size, format.sample_rate);
	
		sprintf(path, "out/%c_dsb-sc.csv", channels[i]);
		print_frequency_domain_to_csv(path, dsb_sc_fft, fft_size, format.sample_rate);

		sprintf(path, "out/%c_ssb-usb.csv", channels[i]);
		print_frequency_domain_to_csv(path, ssb_usb_fft, fft_size, format.sample_rate);

		sprintf(path, "out/%c_ssb-lsb.csv", channels[i]);
		print_frequency_domain_to_csv(path, ssb_lsb_fft, fft_size, format.sample_rate);

		fflush(stdout);

		free(dsb_lc_fft);
		free(dsb_sc_fft);
		free(ssb_usb_fft);
		free(ssb_lsb_fft);

// lb_demodulation:

		// Demodulation
		demodulated_dsb_lc = demodulate_with_envelope_detection_malloc(dsb_lc, sample.size
			, carrier + i, KA);
		demodulated_dsb_sc = demodulate_with_coherent_detection_malloc(dsb_sc, sample.size
			, carrier + i, format.sample_rate);
		demodulated_ssb_usb = demodulate_with_coherent_detection_malloc(ssb_usb, sample.size
			, carrier + i, format.sample_rate);
		demodulated_ssb_lsb = demodulate_with_coherent_detection_malloc(ssb_lsb, sample.size
			, carrier + i, format.sample_rate);
		// Save as wav format
		sprintf(path, "out/%c_dsb-lc.wav", channels[i]);
		save_as_wav_file(path, demodulated_dsb_lc, sample.size, &format);

		sprintf(path, "out/%c_dsb-sc.wav", channels[i]);
		save_as_wav_file(path, demodulated_dsb_sc, sample.size, &format);

		sprintf(path, "out/%c_ssb-usb.wav", channels[i]);
		save_as_wav_file(path, demodulated_ssb_usb, sample.size, &format);

		sprintf(path, "out/%c_ssb-lsb.wav", channels[i]);
		save_as_wav_file(path, demodulated_ssb_lsb, sample.size, &format);

		// Save time domain envleope csv file
		double* env_dm_dsb_lc = detect_envelope_malloc(demodulated_dsb_lc, sample.size);
		double* env_dm_dsb_sc = detect_envelope_malloc(demodulated_dsb_sc, sample.size);
		double* env_dm_ssb_usb = detect_envelope_malloc(demodulated_ssb_usb, sample.size);
		double* env_dm_ssb_lsb = detect_envelope_malloc(demodulated_ssb_lsb, sample.size);

		sprintf(path, "out/%c_dsb-lc_dm-env.csv", channels[i]);
		print_time_domain_to_csv(path, env_dm_dsb_lc, sample.size, format.sample_rate);
		sprintf(path, "out/%c_dsb-sc_dm-env.csv", channels[i]);
		print_time_domain_to_csv(path, env_dm_dsb_sc, sample.size, format.sample_rate);
		sprintf(path, "out/%c_ssb-usb_dm-env.csv", channels[i]);
		print_time_domain_to_csv(path, env_dm_ssb_usb, sample.size, format.sample_rate);
		sprintf(path, "out/%c_ssb-lsb_dm_env.csv", channels[i]);
		print_time_domain_to_csv(path, env_dm_ssb_lsb, sample.size, format.sample_rate);

		complex_t* fft_dm_dsb_lc = fft_malloc(demodulated_dsb_lc, sample.size, &fft_size);
		complex_t* fft_dm_dsb_sc = fft_malloc(demodulated_dsb_sc, sample.size, &fft_size);
		complex_t* fft_dm_ssb_usb = fft_malloc(demodulated_ssb_usb, sample.size, &fft_size);
		complex_t* fft_dm_ssb_lsb = fft_malloc(demodulated_ssb_lsb, sample.size, &fft_size);

		sprintf(path, "out/%c_dsb-lc_dm-fft.csv", channels[i]);
		print_frequency_domain_to_csv(path, fft_dm_dsb_lc, fft_size, format.sample_rate);
		sprintf(path, "out/%c_dsb-sc_dm-fft.csv", channels[i]);
		print_frequency_domain_to_csv(path, fft_dm_dsb_sc, fft_size, format.sample_rate);
		sprintf(path, "out/%c_ssb-usb_dm-fft.csv", channels[i]);
		print_frequency_domain_to_csv(path, fft_dm_ssb_usb, fft_size, format.sample_rate);
		sprintf(path, "out/%c_ssb-lsb_dm-fft.csv", channels[i]);
		print_frequency_domain_to_csv(path, fft_dm_ssb_lsb, fft_size, format.sample_rate);

		free(fft_dm_dsb_lc);
		free(fft_dm_dsb_sc);
		free(fft_dm_ssb_usb);
		free(fft_dm_ssb_lsb);

		free(env_dm_dsb_lc);
		free(env_dm_dsb_sc);
		free(env_dm_ssb_usb);
		free(env_dm_ssb_lsb);

		free(demodulated_dsb_lc);
		free(demodulated_dsb_sc);
		free(demodulated_ssb_usb);
		free(demodulated_ssb_lsb);

		free(dsb_lc);
		free(dsb_sc);
		free(ssb_usb);
		free(ssb_lsb);
	}

	release_wav_data(&sample);
	free(input);

	printf("Done!\n");
	return 0;
}

wav_data_t load_sample(const char* path, format_t* out_header)
{
	FILE* sample = fopen(path, "rb");
	assert(sample != NULL);
	wav_data_t res = { 0, };
	fseek(sample, 16, SEEK_SET);
	fread(out_header, sizeof(format_t), 1, sample);

	fseek(sample, out_header->block_size + 24, SEEK_SET);
	unsigned int data_chunk_size;
	fread(&data_chunk_size, sizeof(unsigned int), 1, sample);
	res.data = malloc(data_chunk_size);
	if (res.data == NULL)
	{
		goto lb_malloc_fail;
	}
	res.size = data_chunk_size / sizeof(short);
	fread(res.data, sizeof(short), res.size, sample);


	fclose(sample);
	return res;

lb_malloc_fail:
	fclose(sample);
	assert(res.data != NULL);
	return res;
}

void release_wav_data(wav_data_t* data)
{
	free(data->data);
}

complex_t complex_add(const complex_t* a, const complex_t* b)
{
	complex_t res = { 0.0, 0.0 };
	res.real = a->real + b->real;
	res.imag = a->imag + b->imag;
	return res;
}

complex_t complex_sub(const complex_t* a, const complex_t* b)
{
	complex_t res = { 0.0, 0.0 };
	res.real = a->real - b->real;
	res.imag = a->imag - b->imag;
	return res;
}

complex_t complex_mul(const complex_t* a, const complex_t* b)
{
	complex_t res = { 0.0, 0.0 };
	res.real = a->real * b->real - a->imag * b->imag;
	res.imag = a->real * b->imag + b->real * a->imag;
	return res;
}

complex_t complex_exp(double theta)
{
	complex_t res = { 0.0, 0.0 };
	res.real = cos(theta);
	res.imag = sin(theta);
	return res;
}

double complex_mag(const complex_t* a)
{
	return sqrt(a->real * a->real + a->imag * a->imag);
}

int* br_order_malloc(int bit_size)
{
	int size = 1 << bit_size;
	int* res = malloc(sizeof(int) * size);
	assert(res != NULL);

	int bit;
	int temp;

	for (int i = 0; i < size; ++i)
	{
		temp = 0;
		for (int j = 0; j < bit_size; ++j)
		{
			bit = (i >> j) & 1;
			temp = (temp << 1) | bit;
		}
		res[i] = temp;
	}

	return res;
}

complex_t* fft_malloc(const double* input, size_t samples, size_t* out_size)
{
	const double PI = acos(-1);
	const complex_t RE1 = { 1.0, 0.0 };
	// br_order copy
	int bit_size = (int)ceil(log2((double)samples));
	size_t size = 1ull << bit_size;
	complex_t* res = malloc(sizeof(complex_t) * size);
	assert(res != NULL);

	int* br_order = br_order_malloc(bit_size);
	int idx;
	int i;
	int j;
	int k;
	*out_size = size;

	for (i = 0; i < size; ++i)
	{
		idx = br_order[i];
		if (idx < samples)
		{
			res[i].real = input[idx];
			res[i].imag = 0.0;
		}
		else
		{
			res[i].real = 0.0;
			res[i].imag = 0.0;
		}
	}

	for (i = 0; i < bit_size; ++i)
	{
		int m = 2 << i;
		int m_half = m >> 1;
		complex_t wm = complex_exp(-2 * PI / (double)m);
		for (j = 0; j < size; j += m)
		{
			complex_t w = RE1;
			for (k = 0; k < m_half; ++k)
			{
				complex_t temp0 = complex_mul(&w, res + j + k + m_half);
				complex_t temp1 = res[j + k];
				res[j + k] = complex_add(&temp1, &temp0);
				res[j + k + m_half] = complex_sub(&temp1, &temp0);
				w = complex_mul(&w, &wm);
			}
		}
	}

	free(br_order);

	return res;
}

complex_t* ifft_malloc(const complex_t* freq_domain, size_t n)
{
	int bit_size = (int)ceil(log2((double)n));
	int length = 1 << bit_size;
	complex_t* res = malloc(sizeof(complex_t) * length);
	assert(res != NULL);

	int* br_order = br_order_malloc(bit_size);
	int i;
	int j;
	int idx;
	int m;
	int m_half;
	complex_t wm;
	complex_t w;
	const complex_t RE1 = { 1.0, 0.0 };
	const double PI = acos(-1);
	complex_t temp0;
	complex_t temp1;


	for (i = 0; i < length; ++i)
	{
		idx = br_order[i];
		res[i] = freq_domain[idx];
	}

	m = 2;
	while (m <= n)
	{
		m_half = m >> 1;
		wm = complex_exp(2 * PI / m);

		for (i = 0; i < n; i += m)
		{
			w = RE1;

			for (j = 0; j < m_half; ++j)
			{
				temp0 = complex_mul(&w, res + i + j + m_half);
				temp1 = res[i + j];

				res[i + j] = complex_add(&temp1, &temp0);
				res[i + j + m_half] = complex_sub(&temp1, &temp0);

				w = complex_mul(&w, &wm);
			}
		}
		m = m << 1;
	}

	for (i = 0; i < length; ++i)
	{
		res[i].real = res[i].real / n;
		res[i].imag = res[i].imag / n;
	}

	free(br_order);

	return res;
}

void print_frequency_domain_to_csv(const char* path, const complex_t* data, size_t size, double fs)
{
	FILE* file = fopen(path, "w");
	assert(file != NULL);

	fprintf(file, "Frequency,Magnitude\n");

	for (size_t i = 0; i < size; ++i)
	{
		double freq;
		if (i < size / 2)
		{
			freq = (double)i * fs / size;
		}
		else
		{
			freq = ((double)i - size) * fs / size;
		}
		fprintf(file, "%lf,%lf\n", freq, complex_mag(&data[i]));
	}
	fclose(file);

}

void print_time_domain_to_csv(const char* path, const double* data, size_t size, double fs)
{
	FILE* file = fopen(path, "w");
	assert(file != NULL);
	
	fprintf(file, "time,signal\n");

	for (size_t i = 0; i < size; ++i)
	{
		double time = (double)i / fs;
		fprintf(file, "%lf,%lf\n", time, data[i]);
	}
	fclose(file);
}

double* am_dsb_lc_malloc(const double* input, size_t samples, double fs, const carrier_t* carrier, double ka)
{
	assert(carrier->frequency * 2 < fs);
	double* res = malloc(sizeof(double) * samples);
	assert(res != NULL);

	const double PI = acos(-1);

	for (size_t i = 0; i < samples; ++i)
	{
		res[i] = carrier->amplitude * (1.0 + ka * input[i]) 
			* cos(2 * PI * carrier->frequency * i / fs + carrier->phase);
	}

	return res;
}

double* am_dsb_sc_malloc(const double* input, size_t samples, double fs, const carrier_t* carrier)
{
	assert(carrier->frequency * 2 < fs);
	double* res = malloc(sizeof(double) * samples);
	assert(res != NULL);

	const double PI = acos(-1);

	for (size_t i = 0; i < samples; i++)
	{
		res[i] = carrier->amplitude 
			* cos(2 * PI * carrier->frequency * i / fs + carrier->phase)
			* input[i];
	}

	return res;
}

double* am_ssb_malloc(const double* input, size_t samples, double fs
	, const carrier_t* carrier, sideband_t sideband)
{
	assert(carrier->frequency * 2 < fs);
	const double PI = acos(-1.0);
	size_t fft_size;
	complex_t* freq_domain;
	double freq;
	size_t i;
	double theta;
	double* res;
	complex_t* analytic_signal;
	double* dsb_sc;
	complex_t* time_domain;

	if (g_ssb_use_filter)
	{
		goto lb_ssb_bandpass_filter;
	}

// lb_ssb_analytic_signal:

	freq_domain = fft_malloc(input, samples, &fft_size);
	
	for (i = 1; i < fft_size; ++i)
	{
		if (i < (fft_size >> 1))
		{
			freq_domain[i].real *= 2.0;
			freq_domain[i].imag *= 2.0;
		}
		else if (i > (fft_size >> 1))
		{
			freq_domain[i].real = 0.0;
			freq_domain[i].imag = 0.0;
		}	
	}

	analytic_signal = ifft_malloc(freq_domain, fft_size);
	free(freq_domain);
	res = malloc(sizeof(double) * samples);
	assert(res != NULL);
		
	switch (sideband)
	{
	case SIDEBAND_USB:
		for (i = 0; i < samples; ++i)
		{
			theta = 2 * PI * carrier->frequency * i / fs + carrier->phase;
			res[i] = carrier->amplitude
				* (analytic_signal[i].real * cos(theta)
				- analytic_signal[i].imag * sin(theta));
		}
		break;

	case SIDEBAND_LSB:
		for (i = 0; i < samples; ++i)
		{
			theta = 2 * PI * carrier->frequency * i / fs + carrier->phase;
			res[i] = carrier->amplitude
				* (analytic_signal[i].real * cos(theta)
				+ analytic_signal[i].imag * sin(theta));
		}
		break;

	default:
		assert(0);
		break;
	}	

	free(analytic_signal);

	return res;

lb_ssb_bandpass_filter:
	dsb_sc = am_dsb_sc_malloc(input, samples, fs, carrier);
	freq_domain = fft_malloc(dsb_sc, samples, &fft_size);
	free(dsb_sc);

	switch (sideband)
	{
	case SIDEBAND_USB:
		for (i = 0; i < fft_size; ++i)
		{
			if (i < (fft_size >> 1))
			{
				freq = i * fs / fft_size;
			}
			else
			{
				freq = (fft_size - i) * fs / fft_size;
			}
			if (freq < carrier->frequency || freq > carrier->frequency + g_audio_bandwidth)
			{
				freq_domain[i].real = 0.0;
				freq_domain[i].imag = 0.0;
			}
		}
		break;
	case SIDEBAND_LSB:
		for (i = 0; i < fft_size; ++i)
		{
			if (i < (fft_size >> 1))
			{
				freq = i * fs / fft_size;
			}
			else
			{
				freq = (fft_size - i) * fs / fft_size;
			}

			if (freq > carrier->frequency || freq < carrier->frequency - g_audio_bandwidth)
			{
				freq_domain[i].real = 0.0;
				freq_domain[i].imag = 0.0;
			}
		}
		break;
	default:
		assert(0);
		break;
	}

	time_domain = ifft_malloc(freq_domain, fft_size);
	free(freq_domain);

	res = malloc(sizeof(double) * samples);
	assert(res != NULL);

	for (i = 0; i < samples; ++i)
	{
		res[i] = time_domain[i].real;
	}

	free(time_domain);	

	return res;	
}

double* detect_envelope_malloc(const double* input, size_t samples)
{
	double* res = malloc(sizeof(double) * samples);
	assert(res != NULL);

	size_t fft_size;
	complex_t* fft = fft_malloc(input, samples, &fft_size);
	size_t fft_size_half = fft_size >> 1;

	for (size_t i = 1; i < fft_size; ++i)
	{
		if (i < fft_size_half)
		{
			fft[i].real *= 2;
			fft[i].imag *= 2;
		}
		else if (i > fft_size_half)
		{
			fft[i].real = 0.0;
			fft[i].imag = 0.0;
		}
	}
	complex_t* analytic_signal = ifft_malloc(fft, fft_size);
	free(fft);
	
	for (size_t i = 0; i < samples; ++i)
	{
		res[i] = complex_mag(analytic_signal + i);
	}

	free(analytic_signal);

	return res;
}

double get_awgn(void)
{
	const double PI = acos(-1.0);
	double temp0;
	double temp1;
	double result;

	do 
	{
		temp1 = rand() / (double)RAND_MAX;

	} while (temp1 == 0.0);

	temp0 = cos(2.0 * PI * rand() / (double)RAND_MAX);
	result = sqrt( -2.0 * log(temp1)) * temp0;

	return result;
}

void save_as_wav_file(const char* path, const double* input, size_t size, const format_t* format)
{
	FILE* file = fopen(path, "wb");
	fwrite("RIFF", sizeof(char), 4, file);
	unsigned int riff_size = (unsigned int)(size * sizeof(short) + 0x24);
	fwrite(&riff_size, sizeof(unsigned int), 1, file);
	fwrite("WAVEfmt ", sizeof(char), 8, file);
	fwrite(format, sizeof(format_t), 1, file);
	fwrite("data", sizeof(char), 4, file);
	unsigned int data_size = (unsigned int)(size * sizeof(short));
	fwrite(&data_size, sizeof(unsigned int), 1, file);

	for (size_t i = 0; i < size; ++i)
	{
		short signal = (short)(0x7fff * input[i]);
		fwrite(&signal, sizeof(short), 1, file);
	}

	fclose(file);	
}

double* demodulate_with_envelope_detection_malloc(const double* signal, size_t size
	, const carrier_t* carrier, double ka)
{
	double* res = malloc(sizeof(double) * size);
	double* envelope = detect_envelope_malloc(signal, size);
	
	for	(size_t i = 0; i < size; ++i)
	{
		res[i] = ((envelope[i] / carrier->amplitude) - 1.0) / ka;
	}
	
	free(envelope);

	return res;
}

double* demodulate_with_coherent_detection_malloc(const double* signal, size_t size
	, const carrier_t* carrier, double fs)
{
	const double PI = acos(-1.0);
	double* res = malloc(sizeof(double) * size);

	double local_oscillator;

	for (size_t i = 0; i < size; ++i)
	{
		local_oscillator = 2 * cos(2 * PI * carrier->frequency * i / fs + carrier->phase);
		res[i] = signal[i] * local_oscillator;
	}

	// LPF
	size_t fft_size;
	complex_t* fft = fft_malloc(res, size, &fft_size);

	for (size_t i = 0; i < fft_size; ++i)
	{
		double freq;
		if (i < (fft_size >> 1))
		{
			freq = i * fs / fft_size;
		}
		else
		{
			freq = (fft_size - i) * fs / fft_size;
		}

		if (g_audio_bandwidth < freq)
		{
			fft[i].real = 0.0;
			fft[i].imag = 0.0;
		}
	}
	complex_t* ifft = ifft_malloc(fft, fft_size);
	free(fft);

	for	(size_t i = 0; i < size; ++i)
	{
		res[i] = ifft[i].real;
	}

	free(ifft);

	return res;
}
