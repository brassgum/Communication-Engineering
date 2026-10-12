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
double g_ssb_bandwidth = 20000.0;

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
void print_complex_to_csv(const char* path, const complex_t* data, size_t size, double fs);
double* am_dsb_lc_malloc(const double* input, size_t samples, double fs, const carrier_t* carrier, double ka);
double* am_dsb_sc_malloc(const double* input, size_t samples, double fs, const carrier_t* carrier);
double* am_ssb_malloc(const double* input, size_t samples, double fs
	, const carrier_t* carrier, sideband_t sideband);
double* detect_envelope_malloc(const double* input, size_t samples, double fs, const carrier_t* carrier);
double get_awgn(void);

int main(int argc, char** argv)
{	
	const double PI = acos(-1);

	const int FC = 100000; // 100kHz carrier frequency
	const double AC = 1.0;

	format_t format;
	wav_data_t sample;
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

	complex_t* input_fft = fft_malloc(input, sample.size, &fft_size);
	const complex_t RE0 = { 0.0, 0.0 };

	// LPF

	for (size_t i = 0; i < fft_size; ++i)
	{
		double freq;
		if (i < (fft_size >> 1))
		{
			freq = i * format.sample_rate / fft_size;
		}
		else
		{
			freq = -(i - fft_size) * format.sample_rate / fft_size;
		}
		if (freq > 20000.0)
		{
			input_fft[i] = RE0;
		}
	}
	complex_t* input_fft_ifft = ifft_malloc(input_fft, fft_size);
	for (size_t i = 0; i < sample.size; ++i)
	{
		input[i] = input_fft_ifft[i].real;
	}

	printf("sample mean: %lf\n", mean);

	carrier_t carrier_a;
	carrier_a.amplitude = AC;
	carrier_a.frequency = 50000; // 100 kHz
	carrier_a.phase = 0.0;

	carrier_t carrier_b = carrier_a;

	carrier_t carrier_c;
	carrier_c.amplitude = AC;
	carrier_c.frequency = FC - 1; // 99.999 kHz
	carrier_c.phase = 0.0;

	carrier_t carrier_d;
	carrier_d.amplitude = AC;
	carrier_d.frequency = FC;
	carrier_d.phase = 89.0 * PI / 180.0;
	
	double* ssb_usb_output = am_ssb_malloc(input, sample.size, format.sample_rate, &carrier_a, SIDEBAND_USB);
	
	complex_t* ssb_usb_fft = fft_malloc(ssb_usb_output, sample.size, &fft_size);
	print_complex_to_csv("out/ssb-usb_output.csv", ssb_usb_fft, fft_size, format.sample_rate);
	
	double* ssb_lsb_output = am_ssb_malloc(input, sample.size, format.sample_rate, &carrier_a, SIDEBAND_LSB);
	complex_t* ssb_lsb_fft = fft_malloc(ssb_lsb_output, sample.size, &fft_size);
	print_complex_to_csv("out/ssb-lsb_output.csv", ssb_lsb_fft, fft_size, format.sample_rate);	

	printf("hilbert done!\n");
	g_ssb_use_filter = 1;

	double* ssb_usb_bpf = am_ssb_malloc(input, sample.size, format.sample_rate
		, &carrier_a, SIDEBAND_USB);

	double* ssb_lsb_bpf = am_ssb_malloc(input, sample.size, format.sample_rate
		, &carrier_a, SIDEBAND_LSB);

	complex_t* usb_bpf_fft = fft_malloc(ssb_usb_bpf, sample.size, &fft_size);
	print_complex_to_csv("out/ssb-usb-bpf_output.csv", usb_bpf_fft, fft_size, format.sample_rate);
	complex_t* lsb_bpf_fft = fft_malloc(ssb_lsb_bpf, sample.size, &fft_size);
	print_complex_to_csv("out/ssb-lsb-bpf_output.csv", lsb_bpf_fft, fft_size, format.sample_rate);

	release_wav_data(&sample);
	free(input);
	free(ssb_usb_output);
	free(ssb_usb_fft);
	free(ssb_lsb_output);
	free(ssb_lsb_fft);
	free(ssb_usb_bpf);
	free(ssb_lsb_bpf);
	free(usb_bpf_fft);
	free(lsb_bpf_fft);

	printf("Done!\n");
	return 0;
}

wav_data_t load_sample(const char* path, format_t* out_header)
{
	FILE* sample = fopen(path, "rb");
	assert(sample != NULL);
	wav_data_t res;
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
}

void release_wav_data(wav_data_t* data)
{
	free(data->data);
}

complex_t complex_add(const complex_t* carrier_a, const complex_t* carrier_b)
{
	complex_t res;
	res.real = carrier_a->real + carrier_b->real;
	res.imag = carrier_a->imag + carrier_b->imag;
	return res;
}

complex_t complex_sub(const complex_t* carrier_a, const complex_t* carrier_b)
{
	complex_t res;
	res.real = carrier_a->real - carrier_b->real;
	res.imag = carrier_a->imag - carrier_b->imag;
	return res;
}

complex_t complex_mul(const complex_t* carrier_a, const complex_t* carrier_b)
{
	complex_t res;
	res.real = carrier_a->real * carrier_b->real - carrier_a->imag * carrier_b->imag;
	res.imag = carrier_a->real * carrier_b->imag + carrier_b->real * carrier_a->imag;
	return res;
}

complex_t complex_exp(double theta)
{
	complex_t res;
	res.real = cos(theta);
	res.imag = sin(theta);
	return res;
}

double complex_mag(const complex_t* carrier_a)
{
	return sqrt(carrier_a->real * carrier_a->real + carrier_a->imag * carrier_a->imag);
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
	static const double PI = acos(-1);
	const complex_t RE1 = { 1.0, 0.0 };
	// br_order copy
	int bit_size = (int)ceil(log2(samples));
	size_t size = 1 << bit_size;
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
	int bit_size = (int)ceil(log2(n));
	int length = 1 << bit_size;
	complex_t* res = malloc(sizeof(complex_t) * length);
	assert(res != NULL);

	int* br_order = br_order_malloc(bit_size);
	int i;
	int j;
	int k;
	int idx;
	int m;
	int m_half;
	complex_t wm;
	complex_t w;
	const complex_t RE1 = { 1.0, 0.0 };
	static const double PI = acos(-1);
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

void print_complex_to_csv(const char* path, const complex_t* data, size_t size, double fs)
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

double* am_dsb_lc_malloc(const double* input, size_t samples, double fs, const carrier_t* carrier, double ka)
{
	assert(carrier->frequency * 2 < fs);
	double* res = malloc(sizeof(double) * samples);
	assert(res != NULL);

	static const double PI = acos(-1);

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

	static const double PI = acos(-1);

	for (size_t i = 0; i < samples; i++)
	{
		res[i] = carrier->amplitude 
			* cos(2 * PI * carrier->frequency * i / fs + carrier->phase)
			* input[i];
	}

	return res;
}

double* am_ssb_malloc(const double* input, size_t samples, double fs, const carrier_t* carrier, sideband_t sideband)
{
	assert(carrier->frequency * 2 < fs);
	static const double PI = acos(-1.0);
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

lb_ssb_analytic_signal:

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
				freq = -(i - fft_size) * fs / fft_size;
			}
			if (freq < carrier->frequency || freq > carrier->frequency + g_ssb_bandwidth)
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
				freq = -(i - fft_size) * fs / fft_size;
			}

			if (freq > carrier->frequency || freq < carrier->frequency - g_ssb_bandwidth)
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

double* noise_malloc(size_t samples, double amplitude)
{
	assert(0);
	double* res = malloc(sizeof(double) * samples);

	return res;
}

double* detect_envelope_malloc(const double* input, size_t samples, double fs, const carrier_t* carrier)
{
	assert(0);
	double* res = malloc(sizeof(double) * samples);

	return res;
}

double get_awgn(void)
{
	static const double PI = acos(-1.0);
	double temp0
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

