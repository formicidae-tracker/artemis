package main

import (
	"bytes"
	"fmt"
	"html/template"
	"io"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"regexp"

	"github.com/jessevdk/go-flags"
)

type Options struct {
	Input  []string `short:"I" long:"input" description:"files to input" required:"true"`
	Output string   `short:"O" long:"output" description:"output file basename without extension" required:"true"`
}

func BuildOutputPaths(base string) (string, string, error) {
	ext := filepath.Ext(base)
	if len(ext) != 0 {
		return "", "", fmt.Errorf("base path contains extension")
	}
	return base + ".h", base + ".c", nil
}

var rx = regexp.MustCompile(`[\. \-~]`)

type Resource struct {
	FileName     string
	VariableName string
	Data         []byte
}

func ResourceVariableName(file string) string {
	base := filepath.Base(file)
	return rx.ReplaceAllString(base, "_")
}

func OpenResource(file string) (Resource, error) {
	f, err := os.Open(file)
	if err != nil {
		return Resource{}, err
	}
	defer f.Close()
	b, err := ioutil.ReadAll(f)
	if err != nil {
		return Resource{}, err
	}
	return Resource{
		FileName:     file,
		VariableName: ResourceVariableName(file),
		Data:         b,
	}, nil
}

var variableSourceTemplate *template.Template
var variableHeaderTemplate *template.Template

var sourceTemplate *template.Template
var headerTemplate *template.Template

func init() {
	var err error
	variableSourceTemplate, err = template.New("variable-source").Parse(`
const unsigned char {{.VariableName}}[{{.Data | len}}] = { {{range .Data}} {{ . | printf "0x%02x"}},{{end}} };
const unsigned int {{.VariableName}}_size = {{.Data | len}};
`)
	if err != nil {
		panic(err.Error())
	}

	variableHeaderTemplate, err = template.New("variable-header").Parse(`
extern const unsigned char {{.VariableName}}[{{.Data | len }}];
extern const unsigned int {{.VariableName}}_size;\
`)
	if err != nil {
		panic(err.Error())
	}

	headerTemplate, err = template.New("header").Parse(`#pragma once

// Generated automatically by resourcify

#ifdef __cplusplus
extern "C"{
#endif //_cplusplus

{{range $key, $value := .Resources}}
{{ $value.Header }}
{{end}}

#ifdef __cplusplus
}
#endif //_cplusplus
`)
	if err != nil {
		panic(err.Error())
	}

	sourceTemplate, err = template.New("source").Parse(`#include "{{.HeaderFile}}"

// Generated automatically by resourcify

#ifdef __cplusplus
extern "C"{
#endif //_cplusplus

{{range $key, $value := .Resources}}
{{ $value.Source }}
{{end}}

#ifdef __cplusplus
}
#endif //_cplusplus
`)
	if err != nil {
		panic(err.Error())
	}

}

type Data struct {
	Resources  map[string]Resource
	HeaderFile string
}

func (r Resource) Header() string {
	buffer := bytes.NewBuffer(nil)
	err := variableHeaderTemplate.Execute(buffer, r)
	if err != nil {
		return ""
	}
	return string(buffer.Bytes())
}

func (r Resource) Source() string {
	buffer := bytes.NewBuffer(nil)
	err := variableSourceTemplate.Execute(buffer, r)
	if err != nil {
		return ""
	}
	return string(buffer.Bytes())
}

func (d *Data) ExecuteTemplates(hw, cw io.Writer) error {
	err := sourceTemplate.Execute(cw, d)
	if err != nil {
		return err
	}
	err = headerTemplate.Execute(hw, d)
	return err
}

func Execute() error {
	opts := Options{}
	if _, err := flags.Parse(&opts); err != nil {
		return err
	}

	hPath, cPath, err := BuildOutputPaths(opts.Output)
	if err != nil {
		return err
	}

	data := Data{
		Resources:  make(map[string]Resource),
		HeaderFile: filepath.Base(hPath),
	}

	for _, i := range opts.Input {
		r, err := OpenResource(i)
		if err != nil {
			return err
		}
		if existing, ok := data.Resources[r.VariableName]; ok == true {
			return fmt.Errorf("Conflicting resource filename: '%s' and '%s' will produce variable '%s'",
				r.FileName, existing.FileName, r.VariableName)
		}
		data.Resources[r.VariableName] = r
	}

	hFile, err := os.Create(hPath)
	if err != nil {
		return err
	}
	defer hFile.Close()
	cFile, err := os.Create(cPath)
	if err != nil {
		return err
	}
	defer cFile.Close()

	return data.ExecuteTemplates(hFile, cFile)
}

func main() {
	if err := Execute(); err != nil {
		log.Fatalf("Unhandled error: %s", err)
	}
}
