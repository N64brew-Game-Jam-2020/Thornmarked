package main

import (
	"archive/zip"
	"bytes"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

var wsdir string

// runGit runs git and returns standard output.
func runGit(args ...string) ([]byte, error) {
	var out bytes.Buffer
	cmd := exec.Command("git", args...)
	cmd.Stdout = &out
	cmd.Stderr = os.Stderr
	cmd.Dir = wsdir
	if err := cmd.Run(); err != nil {
		return nil, err
	}
	return out.Bytes(), nil
}

// getIsDirty returns true if the repository is dirty (has uncommitted changes).
func getIsDirty() (bool, error) {
	b, err := runGit("status", "--porcelain")
	if err != nil {
		return false, fmt.Errorf("could not check if repository is dirty: %w", err)
	}
	return len(b) != 0, nil
}

// getRevNum gets the revision number (the number of commits).
func getRevNum() (uint64, error) {
	b, err := runGit("rev-list", "--count", "--first-parent", "HEAD")
	if err != nil {
		return 0, fmt.Errorf("could not get revision number: %w", err)
	}
	s := strings.TrimSpace(string(b))
	n, err := strconv.ParseUint(s, 10, 64)
	if err != nil {
		return 0, fmt.Errorf("invalid revision number: %w", err)
	}
	return n, nil
}

func getCommitHash() (string, error) {
	b, err := runGit("rev-parse", "HEAD")
	if err != nil {
		return "", fmt.Errorf("could not get commit hash: %w", err)
	}
	return string(bytes.TrimSpace(b)), nil
}

// splitLine splits text at the first line break.
func splitLine(b []byte) (line, rest []byte) {
	i := bytes.IndexByte(b, '\n')
	if i == -1 {
		return b, nil
	}
	return b[:i], b[i+1:]
}

type commitInfo struct {
	Revision  uint64    `json:"revision"`
	Hash      string    `json:"hash"`
	Timestamp time.Time `json:"timestamp"`
	Message   string    `json:"message"`
}

// getCommitTimestamp gets the commit timestamp of the current commit.
func getCommitInfo() (*commitInfo, error) {
	revnum, err := getRevNum()
	if err != nil {
		return nil, err
	}

	chash, err := getCommitHash()
	if err != nil {
		return nil, err
	}

	b, err := runGit("cat-file", "commit", "HEAD")
	if err != nil {
		return nil, fmt.Errorf("could not get commit info: %w", err)
	}
	headers := make(map[string][]byte)
	for len(b) != 0 {
		var line []byte
		line, b = splitLine(b)
		if len(line) == 0 {
			break
		}
		var name, value []byte
		if i := bytes.IndexByte(line, ' '); i != -1 {
			name = line[:i]
			value = line[i+1:]
		} else {
			name = line
		}
		headers[string(name)] = value
	}
	msg := string(b)
	committer := headers["committer"]
	if committer == nil {
		return nil, errors.New("could not find committer")
	}
	cfields := bytes.Fields(committer)
	if len(cfields) < 2 {
		return nil, errors.New("could not parse committer")
	}
	ststamp := cfields[len(cfields)-2]
	stoff := cfields[len(cfields)-1]
	tstamp, err := strconv.ParseInt(string(ststamp), 10, 64)
	if err != nil {
		return nil, fmt.Errorf("invalid commit timestamp %q: %w", ststamp, err)
	}
	toff, err := strconv.ParseInt(string(stoff), 10, 64)
	if err != nil {
		return nil, fmt.Errorf("invalid time zone offset %q: %w", stoff, err)
	}
	toffh := toff / 100
	toffm := toff % 100
	loc := time.FixedZone("", int(toffh*60*60+toffm*60))
	ctime := time.Unix(tstamp, 0).In(loc)

	return &commitInfo{
		Revision:  revnum,
		Hash:      chash,
		Timestamp: ctime,
		Message:   msg,
	}, nil
}

type buildInfo struct {
	commitInfo
	BuildTimestamp time.Time `json:"buildTimestamp"`
}

// buildArtifact builds the desired artifact.
func buildArtifact(args ...string) error {
	fmt.Fprintln(os.Stderr, "Building...")
	cmd := exec.Command("bazel", "build")
	cmd.Args = append(cmd.Args, args...)
	cmd.Stderr = os.Stderr
	cmd.Dir = wsdir
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("build failed: %w", err)
	}
	return nil
}

type pkg struct {
	filename string
	ofp      *os.File
	zfp      *zip.Writer
	tstamp   time.Time
}

func createPackage(filename string) (*pkg, error) {
	ofp, err := ioutil.TempFile(filepath.Dir(filename), "temp.*.zip")
	if err != nil {
		return nil, err
	}
	return &pkg{filename, ofp, zip.NewWriter(ofp), time.Time{}}, nil
}

func (pk *pkg) putInfo(info *buildInfo) error {
	dinfo, err := json.Marshal(info)
	if err != nil {
		return err
	}
	w, err := pk.zfp.CreateHeader(&zip.FileHeader{
		Name:     "INFO.json",
		Method:   zip.Deflate,
		Modified: info.BuildTimestamp,
	})
	if err != nil {
		return err
	}
	if _, err := w.Write(dinfo); err != nil {
		return err
	}
	pk.tstamp = info.BuildTimestamp
	return nil
}

func (pk *pkg) addFile(dest, src string) error {
	fp, err := os.Open(src)
	if err != nil {
		return err
	}
	defer fp.Close()
	w, err := pk.zfp.CreateHeader(&zip.FileHeader{
		Name:     dest,
		Method:   zip.Deflate,
		Modified: pk.tstamp,
	})
	if err != nil {
		return err
	}
	_, err = io.Copy(w, fp)
	return err
}

func (pk *pkg) close(ok bool) (err error) {
	if pk.ofp == nil {
		return errors.New("already closed")
	}
	e1 := pk.zfp.Close()
	e2 := pk.ofp.Chmod(0444)
	e3 := pk.ofp.Close()
	switch {
	case e1 != nil:
		err = e1
	case e2 != nil:
		err = e2
	case e3 != nil:
		err = e3
	case ok:
		err = os.Rename(pk.ofp.Name(), pk.filename)
	}
	if err != nil {
		os.Remove(pk.ofp.Name())
	}
	pk.zfp = nil
	pk.ofp = nil
	return nil
}

func mainE() error {
	allowDirty := flag.Bool("allow-dirty", false, "allow upload with a dirty repository")
	flag.Parse()
	target := "game"
	switch args := flag.Args(); len(args) {
	case 0:
	case 1:
		target = args[0]
	default:
		return fmt.Errorf("unexpected argument: %q", args[1])
	}
	wsdir = os.Getenv("BUILD_WORKSPACE_DIRECTORY")
	if wsdir == "" {
		return errors.New("run this command from Bazel")
	}
	if !*allowDirty {
		dirty, err := getIsDirty()
		if err != nil {
			return err
		}
		if dirty {
			return errors.New("repository is dirty")
		}
	}
	cinfo, err := getCommitInfo()
	if err != nil {
		return err
	}
	now := time.Now()
	binfo := buildInfo{commitInfo: *cinfo, BuildTimestamp: now}
	home := os.Getenv("HOME")
	if home == "" {
		return errors.New("$HOME not set")
	}
	outdir := filepath.Join(home, "Documents", "Artifacts")
	suffix := fmt.Sprintf(".r%04d.%s.zip", cinfo.Revision,
		now.UTC().Format("20060102150405"))
	if err := os.MkdirAll(outdir, 0777); err != nil {
		return err
	}

	switch strings.ToLower(target) {
	case "game":
		outfile := filepath.Join(outdir, "Thornmarked.n64"+suffix)
		pk, err := createPackage(outfile)
		if err != nil {
			return err
		}
		defer pk.close(false)
		if err := pk.putInfo(&binfo); err != nil {
			return err
		}
		if err := buildArtifact("-c", "opt", "--platforms=//n64", "//game/n64"); err != nil {
			return err
		}
		fileList := []string{"Thornmarked.elf", "Thornmarked_NTSC.n64", "Thornmarked_PAL.n64"}
		for _, file := range fileList {
			if err := pk.addFile(file, filepath.Join(wsdir, "bazel-bin/game/n64", file)); err != nil {
				return err
			}
		}
		fmt.Println("Output:", outfile)
		return pk.close(true)

	case "audio":
		outfile := filepath.Join(outdir, "Audio.n64"+suffix)
		pk, err := createPackage(outfile)
		if err != nil {
			return err
		}
		defer pk.close(false)
		if err := pk.putInfo(&binfo); err != nil {
			return err
		}
		if err := buildArtifact("-c", "opt", "--platforms=//n64", "//experimental/audio"); err != nil {
			return err
		}
		fileList := []string{"audio.elf", "audio.n64"}
		for _, file := range fileList {
			if err := pk.addFile(file, filepath.Join(wsdir, "bazel-bin/experimental/audio", file)); err != nil {
				return err
			}
		}
		fmt.Println("Output:", outfile)
		return pk.close(true)

	default:
		return fmt.Errorf("unknown target: %q (valid targets: game, audio)", target)
	}
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
