#include <Python.h>
#include <object.h>

#include "lldc/c_interface.h"

//static PyObject *translator_error;

#define LOG_WARN(x, ...) printf("%s:%d WARN - " x "\n", __FILE__, __LINE__, ##__VA_ARGS__)

//Stuff that allows us to compile with both cc and c++ ...
#ifdef __cplusplus
#define CONST_CAST(type, val) const_cast<type>(val)

extern "C" {
    PyMODINIT_FUNC PyInit_binary_translator(void);
}
#else
#define CONST_CAST(type, val) ((type) (val))
#endif /* __cplusplus */


static unsigned py_read_code_memory(void *opaque, uint64_t address, unsigned size, uint8_t * buffer) 
{
    PyObject *callback = (PyObject *) opaque;
    PyObject *args;
    PyObject *py_data;
    PyObject *py_memoryview;
    Py_buffer py_buffer;
    unsigned actual_size;
    
    memset(&py_buffer, 0, sizeof(Py_buffer));
    assert(PyCallable_Check(callback) && "Python callback needs to be a callable");
    
    args = Py_BuildValue("KK", address, size);
    py_data = PyEval_CallObject(callback, args);
    
    if (!py_data) {
        PyErr_SetString(PyExc_Exception, "Python callback returned NULL pointer");
        LOG_WARN("Python callback returned NULL pointer");
        return 0;
    }
    
    if (!PyObject_CheckBuffer(py_data)) {
        PyErr_SetString(PyExc_TypeError, "Error getting buffer interface for data returned from callback");
        LOG_WARN("Error getting buffer interface for data returned from callback");
        return 0;
    }
    
    if (PyObject_GetBuffer(py_data, &py_buffer, PyBUF_SIMPLE)) {
        PyErr_SetString(PyExc_TypeError, "Error getting buffer interface for data returned from callback");
        LOG_WARN("Error getting buffer interface for data returned from callback");
        return 0;
    }
    
    actual_size = size > py_buffer.len ? py_buffer.len : size;
    memcpy(buffer, py_buffer.buf, actual_size);
    
    PyBuffer_Release(&py_buffer);
    
    Py_XDECREF(py_data);
    
//     printf("Retrieved %d bytes from buffer\n", actual_size);
//     {
//         unsigned i;
//         for (i = 0; i < actual_size; i++) {
//             printf("Buffer byte %d: 0x%02x\n", i, buffer[i] & 0xFF);
//         }
//     }
    return actual_size;
}

static PyObject * py_instrument_memory_access(PyObject *self, PyObject *args, PyObject *kw)
{
    static const char * KEYWORDS[] = {
        "architecture",
        "entry_point",
        "valid_pc_ranges",
        "generated_code_address",
        "get_code_callback",
        "opts",
        NULL
    };
    
    const char * architecture;
    PyObject *py_opts;

    PY_LONG_LONG entry_point;
    PyObject *py_valid_pc_ranges;
    ProgramCounterRange * pc_ranges;
    PY_LONG_LONG generated_code_address;
    PyObject *py_get_code_callback;
    PyObject *py_generated_code;
    PyObject *return_value;
    Py_ssize_t num_pc_ranges;
    Py_ssize_t i;
    GeneratedCode generated_code;
    int err;
    Py_ssize_t dict_pos = 0;
    PyObject *dict_key;
    PyObject *dict_value;
    DictionaryElement* opts;
    
    
    assert(sizeof(PY_LONG_LONG) >= sizeof(uint64_t) && "Size of PY_LONG_LONG less than uint64_t - check your compiler");
    assert(sizeof(PY_LONG_LONG) >= sizeof(void *) && "Size of PY_LONG_LONG is less than void * - check your compiler");
    
    if (!PyArg_ParseTupleAndKeywords(args, 
                                     kw, 
                                     "sKO!KO|O!", 
                                     CONST_CAST(char **, KEYWORDS), 
                                     &architecture, 
                                     &entry_point,
                                     &PyList_Type,
                                     &py_valid_pc_ranges,
                                     &generated_code_address,
                                     &py_get_code_callback,
                                     &PyDict_Type,
                                     &py_opts)) {
        //Error is already set
        return NULL;
    }
    
    //Verify that get_code_callback is a callable
    if (!PyCallable_Check(py_get_code_callback)) {
        PyErr_SetString(PyExc_TypeError, "get_code_callback must be a callable");
        return NULL;
    }
    
    //Parse PC ranges
    num_pc_ranges = PyList_Size(py_valid_pc_ranges);
    pc_ranges = (ProgramCounterRange *) malloc(sizeof(ProgramCounterRange) * (num_pc_ranges + 1));
    
    if (!pc_ranges) {
        PyErr_SetString(PyExc_Exception, "malloc failed");
        return NULL;
    }
    
    memset(pc_ranges, 0, sizeof(ProgramCounterRange) * (num_pc_ranges + 1));
    
    for (i = 0; i < num_pc_ranges; i++) {
        PyObject *tuple;
        PyObject *val;
        
        tuple = PyList_GetItem(py_valid_pc_ranges, i);
        
        if (!PyTuple_Check(tuple) || PyTuple_Size(tuple) != 2) {
            PyErr_SetString(PyExc_TypeError, "valid_pc_ranges must be a list of tuples with two elements");
            free(pc_ranges);
            return NULL;
        }

        if (!PyLong_Check(PyTuple_GetItem(tuple, 0)) || !PyLong_Check(PyTuple_GetItem(tuple, 1))) {
            PyErr_SetString(PyExc_TypeError, "valid_pc_ranges must be a list of tuples with two long elements");
            free(pc_ranges);
            return NULL;
        }
        
        pc_ranges[i].start = PyLong_AsUnsignedLongLong(PyTuple_GetItem(tuple, 0));
        pc_ranges[i].end = PyLong_AsUnsignedLongLong(PyTuple_GetItem(tuple, 1));
    }
    
    //Allocate options dictionary
    opts = malloc(sizeof(DictionaryElement) * (PyDict_Size(py_opts) + 1));
    memset(opts, 0, sizeof(DictionaryElement) * (PyDict_Size(py_opts) + 1));
    i = 0;
    while (PyDict_Next(py_opts, &dict_pos, &dict_key, &dict_value))
    {
        PyObject *ascii_string;

        if (!PyUnicode_Check(dict_key) || !PyUnicode_Check(dict_value))
        {
            PyErr_SetString(PyExc_TypeError, "valid_pc_ranges must be a list of tuples with two elements");
            free(pc_ranges);
            free(opts);
            return NULL;
        }

        ascii_string = PyUnicode_AsASCIIString(dict_key);
        if (!ascii_string)
            return NULL;
        opts[i].key = strdup(PyBytes_AsString(ascii_string));
        ascii_string = PyUnicode_AsASCIIString(dict_value);
        if (!ascii_string)
            return NULL;
        opts[i].value = strdup(PyBytes_AsString(ascii_string));
        i += 1;
    }

    
//    printf("Arguments: architecture = '%s', entry_point = 0x%llx, generated_code_address = 0x%llx\n", architecture, entry_point, generated_code_address);
//    for (i = 0; pc_ranges[i].end != 0; i++) {
//        printf("\tpc_range {start = 0x%lx, end = 0x%lx}\n", pc_ranges[i].start, pc_ranges[i].end);
//    }
    
    memset(&generated_code, 0, sizeof(GeneratedCode));
    err = instrument_memory_access(architecture,
                                   entry_point,
                                   pc_ranges,
                                   generated_code_address,
                                   py_read_code_memory,
                                   py_get_code_callback,
                                   &generated_code,
                                   opts);
    
    free(pc_ranges); //Not useful any more - free the earliest possible.
    if (opts)
    {
        for (i = 0; i < PyDict_Size(py_opts); i++)
        {
            if (opts[i].key)
                free((void *) opts[i].key);
            if (opts[i].value)
                free((void *) opts[i].value);
        }

        free(opts);
    }
    
    py_generated_code = PyBytes_FromStringAndSize(generated_code.code, generated_code.size);
    free(generated_code.code);
    
    if (!py_generated_code) {
        PyErr_SetString(PyExc_Exception, "Creating a bytes object from the resulted code failed");
        return NULL;
    }
    
    return_value = Py_BuildValue("{s:s,s:O,s:l}", "architecture", architecture, "generated_code", py_generated_code, "generated_code_address", generated_code_address);
    
    if (!return_value) {
        PyErr_SetString(PyExc_Exception, "Error creating return dictionary");
        return NULL;
    }
    
    return return_value;
}


static PyMethodDef BinaryTranslatorMethods[] = {
    {"instrument_memory_access", (PyCFunction) py_instrument_memory_access, METH_VARARGS  | METH_KEYWORDS,
     "Execute a shell command."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef BinaryTranslatorModule = {
   PyModuleDef_HEAD_INIT,
   "binary_translator",   /* name of module */
   "TODO: No documentation", /* module documentation, may be NULL */
   -1,       /* size of per-interpreter state of the module,
                or -1 if the module keeps state in global variables. */
   BinaryTranslatorMethods
};

PyMODINIT_FUNC PyInit_binary_translator(void)
{
    return PyModule_Create(&BinaryTranslatorModule);
}
