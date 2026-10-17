use nuvio_backend::search_text::*;

#[test]
fn local_matching_folds_case_and_latin_accents_without_changing_provider_text() {
    let mut text = SearchText::default();
    text.set("Café Ștefan Țără".as_bytes());
    assert_eq!(text.text(), "Café Ștefan Țără");
    assert_eq!(local_key(text.text()), "cafe stefan tara");
    assert_eq!(local_key("Şţ âî Ă"), "st ai a");
}
#[test]
fn composed_and_decomposed_latin_names_match() {
    assert_eq!(local_key("Noël Café"), local_key("Noe\u{308}l Cafe\u{301}"));
    assert_eq!(history_key("CAFÉ"), history_key("Cafe\u{301}"));
    assert_ne!(history_key("Café"), history_key("Cafe"));
}
#[test]
fn non_latin_text_is_not_replaced_with_spaces_or_latin_transliterations() {
    assert_eq!(local_key("Друзья Ёлка Ґанг Ігор"), "друзья елка ґанг ігор");
    assert_eq!(local_key("日本 ガ العربية"), "日本 ガ العربية");
    assert!(!local_key("Друзья").contains(' '));
}
#[test]
fn malformed_encodings_replace_each_invalid_byte_with_a_space() {
    for bytes in [
        vec![0xff],
        vec![0xc0, 0xaf],
        vec![0xe0, 0x80, 0xaf],
        vec![0xed, 0xa0, 0x80],
        vec![0xf4, 0x90, 0x80, 0x80],
        vec![0xe6, 0x97],
    ] {
        assert_eq!(decode_input(&bytes), " ".repeat(bytes.len()));
        assert_eq!(character_count(&bytes), bytes.len());
    }
    assert_eq!(decode_input(b"a\xffb"), "a b");
}
#[test]
fn projections_keep_only_the_fitting_utf8_prefix() {
    let input = "日AB";
    assert_eq!(prefix(input, 2), "");
    assert_eq!(prefix(input, 3), "日");
    assert_eq!(prefix(input, 4), "日A");
    assert_eq!(prefix(input, usize::MAX), input);
    assert_eq!(prefix("", 0), "");
}
#[test]
fn backspace_removes_one_complete_character_including_four_byte_text() {
    let mut text = SearchText::default();
    text.set("abș日🚀".as_bytes());
    for expected in ["abș日", "abș", "ab", "a", "", ""] {
        text.backspace();
        assert_eq!(text.text(), expected);
    }
    assert_eq!(last_character_start("abș日🚀".as_bytes()), "abș日".len());
    assert_eq!(last_character_start(b"ab\xff\xff"), 3);
    assert_eq!(last_character_start(b""), 0);
}
#[test]
fn the_query_minimum_counts_characters_instead_of_utf8_bytes() {
    let mut text = SearchText::default();
    for query in ["", "a", "日", "é", "e\u{301}", "  a  "] {
        text.set(query.as_bytes());
        assert!(!text.searchable(), "{query}");
    }
    text.set("日本".as_bytes());
    assert!(text.searchable());
    assert_eq!(character_count("abș日🚀".as_bytes()), 5);
}
#[test]
fn complete_queries_have_no_old_fixed_buffer_limit_and_controls_stay_out_of_requests() {
    let input = "valid title 日 ".repeat(600);
    let mut text = SearchText::default();
    text.append(input.as_bytes());
    assert_eq!(text.text(), input);
    text.append(b"\x00\r\n");
    assert_eq!(text.text(), input);
    text.set(b"a\x00b");
    assert_eq!(text.text(), "a b");
    assert_eq!(clean_query(" \tA\nB\r "), "A B");
    text.clear();
    assert_eq!(text.text(), "");
}

#[test]
fn native_text_fields_receive_safe_prefixes_and_can_edit_in_place() {
    use nuvio_backend::search_text_ffi::*;
    use std::ffi::{CStr, CString};
    let original = CString::new("日AB").unwrap();
    unsafe {
        let mut out = [42 as std::ffi::c_char; 3];
        nuvio_search_text_normalize(original.as_ptr(), out.as_mut_ptr(), out.len());
        assert_eq!(CStr::from_ptr(out.as_ptr()).to_bytes(), b"");
        assert_eq!(nuvio_search_text_count(original.as_ptr()), 3);
        let mut text = CString::new("CAFÉ日").unwrap().into_bytes_with_nul();
        nuvio_search_text_normalize(text.as_ptr().cast(), text.as_mut_ptr().cast(), text.len());
        assert_eq!(
            CStr::from_ptr(text.as_ptr().cast()).to_str().unwrap(),
            "cafe日"
        );
        let count = CStr::from_ptr(text.as_ptr().cast()).to_bytes().len();
        assert_eq!(
            nuvio_search_text_delete_last(text.as_mut_ptr().cast(), count),
            4
        );
        assert_eq!(CStr::from_ptr(text.as_ptr().cast()).to_bytes(), b"cafe");
        nuvio_search_text_normalize(std::ptr::null(), out.as_mut_ptr(), 0);
        assert_eq!(nuvio_search_text_count(std::ptr::null()), 0);
        assert_eq!(nuvio_search_text_delete_last(std::ptr::null_mut(), 0), 0);
    }
}
